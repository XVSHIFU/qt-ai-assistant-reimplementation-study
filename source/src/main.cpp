#include "ai/openaicompatibleprovider.h"
#include "ai/modeldiscoveryservice.h"
#include "app/dialogmanager.h"
#include "app/hotkeyfacade.h"
#include "app/lifecyclecoordinator.h"
#include "app/managerfacade.h"
#include "app/settingfacade.h"
#include "app/startupdecision.h"
#include "app/uipreferences.h"
#include "app/windowfacade.h"
#include "mock/backendmocks.h"
#include "platform/autostartservice.h"
#include "platform/hotkeyservice.h"
#include "platform/rapooaikeyadapter.h"
#include "platform/singleinstanceservice.h"
#include "platform/systemtraycontroller.h"
#include "platform/windowcontroller.h"
#include "privacy/privacy_consent_service.h"
#include "settings/provider_settings.h"
#include "storage/chat_storage.h"
#include "diagnostics/jsonl_logger.h"
#include "data/data_management_service.h"

#include <QApplication>
#include <QCursor>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QImage>
#include <QIcon>
#include <QPointer>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlError>
#include <QQuickItem>
#include <QQuickItemGrabResult>
#include <QQuickView>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QSettings>
#include <QScreen>
#include <QSystemTrayIcon>
#include <QTemporaryDir>
#include <QTimer>
#include <QVariant>
#include <QStandardPaths>
#include <QDebug>
#include <cstdio>
#include <cstdarg>

#ifndef SMARTKEY_APP_VERSION
#define SMARTKEY_APP_VERSION "0.0.0-development"
#endif
#include <memory>

#ifdef Q_OS_WIN
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#endif

namespace {

void writeStandardBytes(bool errorStream, const QByteArray &bytes)
{
#ifdef Q_OS_WIN
    const DWORD streamId = errorStream ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE;
    const HANDLE handle = GetStdHandle(streamId);
    if (handle && handle != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(handle, bytes.constData(), static_cast<DWORD>(bytes.size()), &written, nullptr);
        return;
    }
#endif
    FILE *stream = errorStream ? stderr : stdout;
    std::fwrite(bytes.constData(), 1, static_cast<size_t>(bytes.size()), stream);
    std::fflush(stream);
}

void writeStandardOutput(const char *format, ...)
{
    char buffer[8192];
    va_list arguments;
    va_start(arguments, format);
    const int length = std::vsnprintf(buffer, sizeof(buffer), format, arguments);
    va_end(arguments);
    if (length > 0)
        writeStandardBytes(false, QByteArray(buffer, qMin(length, int(sizeof(buffer) - 1))));
}

void writeStandardError(const char *format, ...)
{
    char buffer[8192];
    va_list arguments;
    va_start(arguments, format);
    const int length = std::vsnprintf(buffer, sizeof(buffer), format, arguments);
    va_end(arguments);
    if (length > 0)
        writeStandardBytes(true, QByteArray(buffer, qMin(length, int(sizeof(buffer) - 1))));
}

void smokeMessageHandler(QtMsgType, const QMessageLogContext &, const QString &message)
{
    QByteArray bytes = message.toLocal8Bit();
    bytes.append('\n');
    writeStandardBytes(true, bytes);
}

QString smokePathFromArguments(const QStringList &arguments, int *errorCode)
{
    const int index = arguments.indexOf(QStringLiteral("--smoke-test"));
    if (index < 0)
        return {};
    if (index + 1 >= arguments.size()) {
        qCritical() << "--smoke-test requires a PNG path";
        if (errorCode) *errorCode = 2;
        return {};
    }
    return QFileInfo(arguments.at(index + 1)).absoluteFilePath();
}

QString argumentValue(const QStringList &arguments, const QString &name, int *errorCode)
{
    const int index = arguments.indexOf(name);
    if (index < 0)
        return {};
    if (index + 1 >= arguments.size() || arguments.at(index + 1).startsWith(QLatin1String("--"))) {
        qCritical().noquote() << name << "requires a path";
        if (errorCode)
            *errorCode = 2;
        return {};
    }
    return QFileInfo(arguments.at(index + 1)).absoluteFilePath();
}

QString firstInstalledFont(const QStringList &preferred, const QString &fallback)
{
    const QStringList installed = QFontDatabase().families();
    for (const QString &family : preferred) {
        if (installed.contains(family, Qt::CaseInsensitive))
            return family;
    }
    return fallback;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    // Production should use the Windows GPU/ANGLE scene graph. Offscreen smoke
    // already selects the software backend through QT_QUICK_BACKEND; this
    // opt-in remains for machines that need a compatibility fallback.
    if (qEnvironmentVariableIntValue("SMARTKEY_SOFTWARE_RENDERER") == 1) {
        QCoreApplication::setAttribute(Qt::AA_UseSoftwareOpenGL);
        QQuickWindow::setSceneGraphBackend(QSGRendererInterface::Software);
    }
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("SmartKey AI"));
    app.setApplicationDisplayName(QStringLiteral("智键 AI"));
    app.setApplicationVersion(QStringLiteral(SMARTKEY_APP_VERSION));
    app.setOrganizationName(QStringLiteral("SmartKeyAI"));
    app.setOrganizationDomain(QStringLiteral("local.smartkeyai"));
    const QString uiFontFamily = firstInstalledFont(
                {QStringLiteral("Segoe UI Variable Text"), QStringLiteral("Segoe UI"),
                 QStringLiteral("Microsoft YaHei UI")},
                app.font().family());
    const QString codeFontFamily = firstInstalledFont(
                {QStringLiteral("Cascadia Mono"), QStringLiteral("Cascadia Code"),
                 QStringLiteral("Consolas")},
                QStringLiteral("monospace"));
    QFont applicationFont(uiFontFamily);
    applicationFont.setPointSizeF(10.0);
    applicationFont.setHintingPreference(QFont::PreferVerticalHinting);
    applicationFont.setStyleStrategy(QFont::PreferAntialias);
    app.setFont(applicationFont);
    app.setProperty("uiFontFamily", uiFontFamily);
    app.setProperty("codeFontFamily", codeFontFamily);
    app.setWindowIcon(QIcon(QStringLiteral(":/Image/Log/com.rapoo.smartkey.ico")));
    app.setQuitOnLastWindowClosed(false);

    int argumentError = 0;
    const QString screenshotPath = smokePathFromArguments(app.arguments(), &argumentError);
    if (argumentError)
        return argumentError;
    const bool smokeTest = !screenshotPath.isEmpty();
    const bool manualUiTest = app.arguments().contains(QStringLiteral("--manual-ui-test"));
    const bool isolatedUiTest = smokeTest || manualUiTest;
    const bool background = app.arguments().contains(QStringLiteral("--background"));

    std::unique_ptr<QTemporaryDir> temporaryUiData;
    QString isolatedDataRoot = argumentValue(app.arguments(),
                                             QStringLiteral("--smoke-data-root"),
                                             &argumentError);
    if (argumentError)
        return argumentError;
    if (smokeTest && isolatedDataRoot.isEmpty()) {
        qCritical() << "--smoke-test requires --smoke-data-root with a unique temporary directory";
        return 2;
    }
    if (manualUiTest && isolatedDataRoot.isEmpty()) {
        temporaryUiData.reset(new QTemporaryDir(
            QDir::temp().filePath(QStringLiteral("SmartKeyAI-manual-ui-XXXXXX"))));
        if (!temporaryUiData->isValid()) {
            qCritical() << "Could not create isolated manual UI data root";
            return 2;
        }
        isolatedDataRoot = temporaryUiData->path();
    }
    if (isolatedUiTest) {
        const QFileInfo isolatedRootInfo(isolatedDataRoot);
        QString validatedSmokeRoot;
        const bool validRoot = smokeTest
                ? smartkey::isSafeSmokeDataRoot(isolatedDataRoot, &validatedSmokeRoot)
                : isolatedRootInfo.isAbsolute() && !isolatedRootInfo.isRoot()
                    && isolatedRootInfo.exists() && isolatedRootInfo.isDir();
        if (!validRoot) {
            qCritical().noquote() << "Invalid isolated UI data root:" << isolatedDataRoot;
            return 2;
        }
        isolatedDataRoot = smokeTest
                ? validatedSmokeRoot
                : QDir::cleanPath(isolatedRootInfo.canonicalFilePath());
        const QString isolatedSettingsRoot = QDir(isolatedDataRoot)
                .filePath(QStringLiteral("config"));
        QDir().mkpath(isolatedSettingsRoot);
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                           isolatedSettingsRoot);
        QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope,
                           isolatedSettingsRoot);
        // GUI-subsystem applications have no CRT console streams. Preserve
        // deterministic smoke diagnostics when the parent redirects the Win32
        // standard handles, without allocating a visible console window.
        qInstallMessageHandler(smokeMessageHandler);
    }

    SingleInstanceService singleInstance;
    if (!isolatedUiTest) {
        QString singleError;
        const auto acquire = singleInstance.acquire(QStringLiteral("activate"), 1200, &singleError);
        if (acquire == SingleInstanceService::SecondaryNotified)
            return 0;
        if (acquire == SingleInstanceService::Failed) {
            qCritical().noquote() << "SINGLE_INSTANCE_ERROR" << singleError;
            return 6;
        }
    }

    // Recovered QML assigns these values after Loader completion.
    app.setProperty("themeManager", QVariant::fromValue<QObject *>(nullptr));
    app.setProperty("fontManager", QVariant::fromValue<QObject *>(nullptr));

    QQuickView view;
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.setColor(Qt::transparent);
    view.resize(576, 720);

    WindowController windowController(&view);
    if (manualUiTest) {
        view.setFlags(Qt::Window | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
        view.setTitle(QStringLiteral("SmartKey AI UI Test"));
    }
    WindowFacade window(&view, &windowController);
    AutoStartService autoStart(QStringLiteral("SmartKey AI"));
    // UI smoke must not even read the real HKCU Run value through QML bindings.
    SettingFacade setting(isolatedUiTest ? nullptr : &autoStart);
    ManagerFacade manager;
    DeviceListMock deviceList;
    const QString configRoot = isolatedUiTest
            ? QDir(isolatedDataRoot).filePath(QStringLiteral("config"))
            : QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    const QString dataRoot = isolatedUiTest
            ? QDir(isolatedDataRoot).filePath(QStringLiteral("data"))
            : QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    smartkey::ProviderSettings providerSettings(
                QDir(configRoot).filePath(QStringLiteral("settings.ini")),
                QDir(configRoot).filePath(QStringLiteral("credentials.ini")));
    smartkey::PrivacyConsentService privacyConsent(
                QDir(configRoot).filePath(QStringLiteral("privacy.ini")));
    // Visual smoke/manual UI runs use an isolated temporary root. Explicitly
    // accept there so legacy visual tests remain deterministic without reading
    // or changing the real user's privacy state.
    if (isolatedUiTest)
        privacyConsent.accept(QStringLiteral("test"));
    providerSettings.setPrivacyConsentService(&privacyConsent);
    QDir().mkpath(dataRoot);
    const QString logPath = QDir(dataRoot).filePath(QStringLiteral("logs/app.jsonl"));
    const QString databasePath = QDir(dataRoot).filePath(QStringLiteral("chat.sqlite"));
    smartkey::JsonlLogger logger(logPath);
    smartkey::ChatStorage storage(databasePath);
    if (!storage.open()) {
        qWarning().noquote() << "STORAGE_OPEN_ERROR" << storage.errorString();
        logger.write(QStringLiteral("storage"), QStringLiteral("open_failed"),
                     {{QStringLiteral("error_class"), QStringLiteral("sqlite_open")}},
                     QStringLiteral("error"));
    } else {
        storage.markIncompleteMessagesInterrupted();
        if (!isolatedUiTest) {
            const QString legacyRoot = QDir(QString::fromLocal8Bit(qgetenv("LOCALAPPDATA")))
                    .filePath(QStringLiteral("SmartKey AI/ChatHistory"));
            if (QFileInfo::exists(QDir(legacyRoot).filePath(QStringLiteral("OverallTable.json")))) {
                const smartkey::LegacyImportResult importResult = storage.importLegacyChatHistory(legacyRoot);
                logger.write(QStringLiteral("storage"), QStringLiteral("legacy_import"),
                             {{QStringLiteral("success"), importResult.success},
                              {QStringLiteral("already_imported"), importResult.alreadyImported},
                              {QStringLiteral("conversations"), importResult.conversationsImported},
                              {QStringLiteral("messages"), importResult.messagesImported}});
            }
        }
    }
    smartkey::ai::OpenAiCompatibleProvider aiProvider;
    smartkey::ai::ModelDiscoveryService modelDiscovery;
    DialogManager dialogManager(&providerSettings, &aiProvider, &storage);
    smartkey::DataManagementService dataManagement(
                &storage, &providerSettings,
                QDir(configRoot).filePath(QStringLiteral("data-management.ini")),
                dataRoot, logPath, app.applicationVersion());
    dialogManager.setPrivacyConsentService(&privacyConsent);
    QObject::connect(&dataManagement, &smartkey::DataManagementService::chatsChanged,
                     &dialogManager, &DialogManager::reloadHistory);
    if (isolatedUiTest) {
        // Keep the visual regression deterministic and exercise the Markdown
        // structures that previously rendered as raw/plain text.
        if (auto *messages = qobject_cast<ChatConversationModel *>(dialogManager.dialogModel())) {
            messages->replaceAll({
                ChatMessage(0, QStringLiteral("请展示 Markdown 渲染能力。"),
                            QString(), QString(), QStringLiteral("complete")),
                ChatMessage(1,
                    QStringLiteral("## Markdown 渲染验收\n\n"
                                   "正文支持 **粗体**、*斜体*、`inline code` 与[安全链接](https://example.com)。\n\n"
                                   "> 引用内容需要有清晰的层级与左侧标识。\n\n"
                                   "| 语法 | 效果 |\n| --- | --- |\n| 标题 | 层级排版 |\n| 代码 | 独立容器 |\n\n"
                                   "```cpp\nconst QString message = QStringLiteral(\"Hello, Markdown!\");\n```\n\n"
                                   "---\n\n- 列表项目一\n- 列表项目二"),
                    QStringLiteral("先验证结构，再验证字体、间距与滚动性能。"),
                    QString(), QStringLiteral("complete"))
            });
        }
        if (auto *history = qobject_cast<ConversationListModel *>(dialogManager.dialogInfoModel())) {
            history->replaceAll({
                ConversationInfo(QStringLiteral("smoke-markdown"),
                                 QStringLiteral("Markdown 渲染验收"), true),
                ConversationInfo(QStringLiteral("smoke-scroll"),
                                 QStringLiteral("长会话滚动测试"), false)
            });
        }
    }
    HotkeyService nativeHotkey;
    HotkeyFacade hotkey(&nativeHotkey);
    RapooAiKeyAdapter rapooKey;
    UiPreferences uiPreferences;
    uiPreferences.setEngine(view.engine());

    window.setAllowQuestion(providerSettings.configured() && privacyConsent.consentGranted());
    QObject::connect(&providerSettings, &smartkey::ProviderSettings::configuredChanged,
                     &window, [&] {
        window.setAllowQuestion(providerSettings.configured()
                                && privacyConsent.consentGranted());
        if (providerSettings.configured() && !providerSettings.startupCompleted())
            providerSettings.setStartupCompleted(true);
    });
    QObject::connect(&providerSettings, &smartkey::ProviderSettings::operationFailed,
                     &app, [&](const QString &operation, const QString &) {
        logger.write(QStringLiteral("settings"), QStringLiteral("operation_failed"),
                     {{QStringLiteral("operation"), operation}}, QStringLiteral("error"));
    });
    QObject::connect(&aiProvider, &smartkey::ai::OpenAiCompatibleProvider::failed,
                     &app, [&](const QString &requestId, const smartkey::ai::AiError &error) {
        logger.write(QStringLiteral("ai.transport"), QStringLiteral("request_failed"),
                     {{QStringLiteral("request_id"), requestId},
                      {QStringLiteral("error_code"), static_cast<int>(error.code)},
                      {QStringLiteral("http_status"), error.httpStatus},
                      {QStringLiteral("native_code"), error.nativeCode}},
                     QStringLiteral("error"));
    });
    QObject::connect(&aiProvider, &smartkey::ai::OpenAiCompatibleProvider::completed,
                     &app, [&](const QString &requestId) {
        logger.write(QStringLiteral("ai.transport"), QStringLiteral("request_completed"),
                     {{QStringLiteral("request_id"), requestId}});
    });
    const QMetaObject::Connection modelDiscoveryRequestConnection = QObject::connect(
                     &providerSettings, &smartkey::ProviderSettings::modelDiscoveryRequested,
                     &app, [&](const QString &profileId) {
        const QVariantMap profile = providerSettings.profile(profileId);
        QByteArray credential;
        const QString auth = profile.value(QStringLiteral("authScheme"),
                                           QStringLiteral("bearer")).toString().toLower();
        if (auth != QLatin1String("none") && auth != QLatin1String("anonymous")) {
            QString credentialError;
            if (!providerSettings.credential(profileId, &credential, &credentialError)) {
                providerSettings.setModelDiscoveryError(profileId, credentialError);
                return;
            }
        }
        modelDiscovery.discover(profileId, profile, credential);
        smartkey::CredentialStore::clearSensitive(&credential);
    });
    QObject::connect(&modelDiscovery, &smartkey::ai::ModelDiscoveryService::discovered,
                     &providerSettings, &smartkey::ProviderSettings::setDiscoveredModels);
    QObject::connect(&modelDiscovery, &smartkey::ai::ModelDiscoveryService::failed,
                     &app, [&](const QString &profileId, const smartkey::ai::AiError &error) {
        providerSettings.setModelDiscoveryError(profileId, error.message);
        logger.write(QStringLiteral("ai.models"), QStringLiteral("discovery_failed"),
                     {{QStringLiteral("error_code"), static_cast<int>(error.code)},
                      {QStringLiteral("http_status"), error.httpStatus}},
                     QStringLiteral("warning"));
    });
    QObject::connect(&providerSettings, &smartkey::ProviderSettings::profilesChanged,
                     &modelDiscovery, &smartkey::ai::ModelDiscoveryService::cancelAll);
    QObject::connect(&providerSettings, &smartkey::ProviderSettings::activeProfileIdChanged,
                     &modelDiscovery, &smartkey::ai::ModelDiscoveryService::cancelAll);

    QTimer modelRefreshTimer;
    modelRefreshTimer.setSingleShot(true);
    modelRefreshTimer.setInterval(650);
    QObject::connect(&modelRefreshTimer, &QTimer::timeout,
                     &providerSettings, &smartkey::ProviderSettings::refreshModels);
    const auto scheduleModelRefresh = [&] {
        if (providerSettings.configured() && privacyConsent.consentGranted())
            modelRefreshTimer.start();
    };
    QObject::connect(&providerSettings, &smartkey::ProviderSettings::profilesChanged,
                     &app, scheduleModelRefresh);
    QObject::connect(&providerSettings, &smartkey::ProviderSettings::activeProfileIdChanged,
                     &app, scheduleModelRefresh);
    QObject::connect(&providerSettings, &smartkey::ProviderSettings::configuredChanged,
                     &app, scheduleModelRefresh);
    logger.write(QStringLiteral("application"), QStringLiteral("started"),
                 {{QStringLiteral("version"), app.applicationVersion()},
                  {QStringLiteral("architecture"), QStringLiteral("x86")},
                  {QStringLiteral("background"), background}});

    view.rootContext()->setContextProperty(QStringLiteral("window"), &window);
    view.rootContext()->setContextProperty(QStringLiteral("dialog_manager"), &dialogManager);
    view.rootContext()->setContextProperty(QStringLiteral("setting"), &setting);
    view.rootContext()->setContextProperty(QStringLiteral("manager"), &manager);
    view.rootContext()->setContextProperty(QStringLiteral("device_list_HL"), &deviceList);
    view.rootContext()->setContextProperty(QStringLiteral("providerSettings"), &providerSettings);
    view.rootContext()->setContextProperty(QStringLiteral("privacyConsent"), &privacyConsent);
    view.rootContext()->setContextProperty(QStringLiteral("dataManagement"), &dataManagement);
    view.rootContext()->setContextProperty(QStringLiteral("hotkeyService"), &hotkey);
    view.rootContext()->setContextProperty(QStringLiteral("uiPreferences"), &uiPreferences);

    QObject::connect(view.engine(), &QQmlEngine::warnings, &app,
                     [](const QList<QQmlError> &warnings) {
        for (const QQmlError &warning : warnings)
            qWarning().noquote() << "QML_WARNING" << warning.toString();
    });

    view.setSource(QUrl(QStringLiteral("qrc:/MainView.qml")));
    if (view.status() == QQuickView::Error || !view.rootObject()) {
        for (const QQmlError &error : view.errors()) {
            qCritical().noquote() << "QML_COMPONENT_ERROR" << error.toString();
            writeStandardError("QML_COMPONENT_ERROR %s\n",
                               qPrintable(error.toString()));
        }
        std::fflush(stderr);
        qCritical() << "SMOKE_ROOT_OBJECT=null";
        return 3;
    }

    writeStandardOutput("ENTRY_URL=qrc:/MainView.qml\n");
    writeStandardOutput("SMOKE_ROOT_OBJECT=non-null\n");
    writeStandardOutput("SMOKE_VIEW_SIZE=%dx%d\n", view.width(), view.height());
    if (isolatedUiTest) {
        writeStandardOutput("SMOKE_DATA_ROOT=%s\n", qPrintable(isolatedDataRoot));
        writeStandardOutput("SMOKE_SETTINGS_PATH=%s\n",
                            qPrintable(providerSettings.settingsFilePath()));
        writeStandardOutput("SMOKE_CREDENTIAL_PATH=%s\n",
                            qPrintable(providerSettings.credentialFilePath()));
        writeStandardOutput("SMOKE_DATABASE_PATH=%s\n", qPrintable(databasePath));
        writeStandardOutput("SMOKE_LOG_PATH=%s\n", qPrintable(logPath));
        writeStandardOutput("SMOKE_LEGACY_IMPORT_ENABLED=0\n");
        writeStandardOutput("SMOKE_AUTOSTART_ACCESS_ENABLED=0\n");
        writeStandardOutput("SMOKE_HOTKEY_STATE_ACCESS_ENABLED=0\n");
    }
    std::fflush(stdout);

    QPointer<QQuickWindow> privacyWindow;
    const auto showPrivacy = [&] {
        bool created = false;
        if (!privacyWindow) {
            QQmlComponent component(view.engine(), QUrl(QStringLiteral("qrc:/firstDiaLog.qml")));
            if (component.isError()) {
                for (const QQmlError &error : component.errors())
                    qWarning().noquote() << "PRIVACY_COMPONENT_ERROR" << error.toString();
                return;
            }
            QObject *object = component.create(view.rootContext());
            privacyWindow = qobject_cast<QQuickWindow *>(object);
            if (!privacyWindow) {
                qWarning() << "PRIVACY_COMPONENT_ERROR root is not a QQuickWindow";
                delete object;
                return;
            }
            privacyWindow->setFlags(Qt::Window | Qt::FramelessWindowHint
                                    | Qt::NoDropShadowWindowHint);
            created = true;
        }
        if (created) {
            QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
            if (!screen)
                screen = QGuiApplication::primaryScreen();
            if (screen) {
                const QRect available = screen->availableGeometry();
                const int width = qBound(520, available.width() - 32, 640);
                const int height = qBound(480, available.height() - 32, 580);
                privacyWindow->resize(width, height);
                privacyWindow->setPosition(
                            available.x() + (available.width() - width) / 2,
                            available.y() + (available.height() - height) / 2);
            }
        }
        privacyWindow->show();
        privacyWindow->raise();
        privacyWindow->requestActivate();
    };

    QPointer<QQuickWindow> settingsWindow;
    const auto showSettings = [&] {
        if (!privacyConsent.consentGranted()) {
            showPrivacy();
            return;
        }
        bool created = false;
        if (!settingsWindow) {
            QQmlComponent component(view.engine(), QUrl(QStringLiteral("qrc:/SettingsPage.qml")));
            if (component.isError()) {
                for (const QQmlError &error : component.errors())
                    qWarning().noquote() << "SETTINGS_COMPONENT_ERROR" << error.toString();
                return;
            }
            QObject *object = component.create(view.rootContext());
            settingsWindow = qobject_cast<QQuickWindow *>(object);
            if (!settingsWindow) {
                qWarning() << "SETTINGS_COMPONENT_ERROR root is not a QQuickWindow";
                delete object;
                return;
            }
            settingsWindow->setFlags(Qt::Window | Qt::FramelessWindowHint
                                     | Qt::NoDropShadowWindowHint);
            created = true;
        }
        if (created) {
            QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
            if (!screen)
                screen = QGuiApplication::primaryScreen();
            if (screen) {
                const QRect available = screen->availableGeometry();
                const int adaptiveWidth = qBound(420, available.width() - 32, 680);
                const int adaptiveHeight = qBound(480, available.height() - 32, 700);
                settingsWindow->resize(adaptiveWidth, adaptiveHeight);
                settingsWindow->setPosition(
                            available.x() + (available.width() - adaptiveWidth) / 2,
                            available.y() + (available.height() - adaptiveHeight) / 2);
            }
        }
        settingsWindow->show();
#ifdef Q_OS_WIN
        // Unlike the tray-only chat window, settings must remain discoverable
        // from the Windows taskbar and Alt+Tab while it is open.
        const HWND settingsHwnd = reinterpret_cast<HWND>(settingsWindow->winId());
        if (settingsHwnd) {
            LONG_PTR style = GetWindowLongPtrW(settingsHwnd, GWL_EXSTYLE);
            style &= ~WS_EX_TOOLWINDOW;
            style |= WS_EX_APPWINDOW;
            SetWindowLongPtrW(settingsHwnd, GWL_EXSTYLE, style);
            SetWindowPos(settingsHwnd, nullptr, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER
                         | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        }
#endif
        settingsWindow->raise();
        settingsWindow->requestActivate();
    };

    QObject::connect(&privacyConsent, &smartkey::PrivacyConsentService::policyDisplayRequested,
                     &app, showPrivacy);
    QObject::connect(&privacyConsent,
                     &smartkey::PrivacyConsentService::consentRequiredForAction,
                     &app, [&](const QString &) { showPrivacy(); });
    QObject::connect(&privacyConsent, &smartkey::PrivacyConsentService::consentChanged,
                     &app, [&] {
        window.setAllowQuestion(providerSettings.configured()
                                && privacyConsent.consentGranted());
        if (!privacyConsent.consentGranted()) {
            modelRefreshTimer.stop();
            modelDiscovery.cancelAll();
            dialogManager.cancelResponse();
            return;
        }
        if (privacyWindow)
            privacyWindow->hide();
        showSettings();
        scheduleModelRefresh();
    });
    QObject::connect(&privacyConsent, &smartkey::PrivacyConsentService::dataDeletionRequested,
                     &app, [&] {
        // Intentionally no deletion here. The signal is the audited hand-off
        // point for a future flow that must present the exact deletion scope.
        logger.write(QStringLiteral("privacy"), QStringLiteral("data_deletion_requested"));
    });

    QObject::connect(&window, &WindowFacade::settingsRequested, &app, showSettings);
    const auto focusChatInput = [&] {
        if (!view.rootObject())
            return;
        QMetaObject::invokeMethod(view.rootObject(), "focusInput", Qt::DirectConnection);
        QQuickItem *input = view.rootObject()->findChild<QQuickItem *>(QStringLiteral("chatInput"));
        if (input)
            input->forceActiveFocus(Qt::OtherFocusReason);
    };
    QObject::connect(&windowController, &WindowController::inputFocusRequested,
                     &app, focusChatInput);
    QObject::connect(&singleInstance, &SingleInstanceService::activationRequested,
                     &app, [&] {
        if (privacyConsent.consentGranted()) windowController.reveal();
        else showPrivacy();
    });
    QObject::connect(&nativeHotkey, &HotkeyService::activated,
                     &app, [&] {
        if (privacyConsent.consentGranted()) windowController.toggleFromHotkey();
        else showPrivacy();
    });
    QObject::connect(&rapooKey, &RapooAiKeyAdapter::activated,
                     &app, [&] {
        if (privacyConsent.consentGranted()) windowController.toggleFromHotkey();
        else showPrivacy();
    });

    QSettings applicationSettings;
    if (!isolatedUiTest) {
        QString hotkeyError;
        if (!nativeHotkey.loadAndRegister(applicationSettings, &hotkeyError))
            qWarning().noquote() << "HOTKEY_REGISTRATION_ERROR" << hotkeyError;
        rapooKey.start(); // Unavailable hardware remains a non-fatal, isolated adapter.
    }

    std::unique_ptr<SystemTrayController> tray;
    if (!isolatedUiTest) {
        tray.reset(new SystemTrayController(&windowController, &autoStart));
        QObject::connect(tray.get(), &SystemTrayController::settingsRequested, &app, showSettings);
        QObject::connect(tray.get(), &SystemTrayController::newChatRequested, &app, [&] {
            if (!privacyConsent.consentGranted()) {
                showPrivacy();
                return;
            }
            dialogManager.addNewChat();
            windowController.reveal();
        });
        tray->show();
    }

    smartkey::LifecycleCoordinator::Hooks lifecycleHooks;
    lifecycleHooks.stopAcceptingRequests = [&] {
        window.setAllowQuestion(false);
        dialogManager.stopAcceptingRequests();
        modelRefreshTimer.stop();
        QObject::disconnect(modelDiscoveryRequestConnection);
    };
    lifecycleHooks.cancelRequests = [&] {
        dialogManager.cancelRequestsForShutdown();
        modelDiscovery.cancelAll();
    };
    lifecycleHooks.flushAndPersist = [&] {
        dialogManager.flushAndPersistShutdownState();
        logger.write(QStringLiteral("application"), QStringLiteral("shutdown_flush"));
    };
    lifecycleHooks.closeStorage = [&] {
        if (!storage.checkpointAndClose()) {
            logger.write(QStringLiteral("storage"), QStringLiteral("checkpoint_failed"),
                         {{QStringLiteral("error_class"), QStringLiteral("sqlite_checkpoint")}},
                         QStringLiteral("warning"));
        }
    };
    lifecycleHooks.unregisterIntegrations = [&] {
        nativeHotkey.disable();
        rapooKey.stop();
        singleInstance.close();
        if (tray) {
            tray->hide();
        }
        if (settingsWindow)
            settingsWindow->close();
        if (privacyWindow)
            privacyWindow->close();
        windowController.beginExit();
    };
    lifecycleHooks.quitApplication = [&] { app.quit(); };
    smartkey::LifecycleCoordinator lifecycle(lifecycleHooks, &app);

    if (tray) {
        QObject::connect(tray.get(), &SystemTrayController::exitRequested,
                         &lifecycle, &smartkey::LifecycleCoordinator::requestTrayExit);
    }
    QObject::connect(&window, &WindowFacade::exitRequested,
                     &lifecycle, &smartkey::LifecycleCoordinator::requestQmlExit);
    QObject::connect(&app, &QCoreApplication::aboutToQuit,
                     &lifecycle, &smartkey::LifecycleCoordinator::requestSystemExit,
                     Qt::DirectConnection);
    // The QQuickView is constructed before its context objects and therefore
    // would otherwise be destroyed after them. Tear down every QML root while
    // its context is still valid to avoid late binding evaluation against
    // already-destroyed services during application shutdown.
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &view, [&] {
        if (settingsWindow)
            delete settingsWindow.data();
        if (privacyWindow)
            delete privacyWindow.data();
        view.setSource(QUrl());
    }, Qt::DirectConnection);

    QObject::connect(&manager, &ManagerFacade::itemSelected, &app, [&](int index) {
        if (index == 0)
            showSettings();
        else if (index == 1)
            lifecycle.requestTrayExit();
    });

    const smartkey::StartupAction startupAction = smartkey::decideStartupAction(
                smokeTest, manualUiTest, background,
                QSystemTrayIcon::isSystemTrayAvailable(),
                providerSettings.configured(), providerSettings.startupCompleted());
    if (!privacyConsent.consentGranted()) {
        showPrivacy();
    } else if (startupAction == smartkey::StartupAction::RunSmoke) {
        showSettings();
        if (!settingsWindow) {
            qCritical() << "SETTINGS_COMPONENT_ERROR root window is null";
            return 7;
        }
        writeStandardOutput("SMOKE_SETTINGS_SIZE=%dx%d\n",
                            settingsWindow->width(), settingsWindow->height());
        writeStandardOutput("SMOKE_SETTINGS_TASKBAR_WINDOW=%d\n",
                            static_cast<int>(settingsWindow->flags() & Qt::WindowType_Mask)
                            == static_cast<int>(Qt::Window));
        const QString settingsScreenshotPath = QFileInfo(screenshotPath).dir()
                .filePath(QStringLiteral("settings_smoke.png"));
        QTimer::singleShot(350, &app, [&app, &settingsWindow, &windowController,
                                      settingsScreenshotPath] {
            if (!settingsWindow) {
                app.exit(9);
                return;
            }
            QDir().mkpath(QFileInfo(settingsScreenshotPath).absolutePath());
            const QImage image = settingsWindow->grabWindow();
            if (!image.isNull() && image.save(settingsScreenshotPath, "PNG")) {
                writeStandardOutput("SMOKE_SETTINGS_SCREENSHOT=%s\n",
                                    qPrintable(settingsScreenshotPath));
                settingsWindow->hide();
                windowController.reveal();
                return;
            }
            const QSharedPointer<QQuickItemGrabResult> result =
                    settingsWindow->contentItem()->grabToImage(settingsWindow->size());
            if (result.isNull()) {
                qCritical() << "SETTINGS_SCREENSHOT_ERROR grab unavailable";
                app.exit(9);
                return;
            }
            QObject::connect(result.data(), &QQuickItemGrabResult::ready, &app,
                             [result, settingsScreenshotPath, &app,
                              &settingsWindow, &windowController] {
                if (result->image().isNull()
                        || !result->image().save(settingsScreenshotPath, "PNG")) {
                    qCritical() << "SETTINGS_SCREENSHOT_ERROR save failed";
                    app.exit(9);
                    return;
                }
                writeStandardOutput("SMOKE_SETTINGS_SCREENSHOT=%s\n",
                                    qPrintable(settingsScreenshotPath));
                settingsWindow->hide();
                windowController.reveal();
            });
        });
        QTimer::singleShot(700, &app, [&app, &view, &providerSettings, screenshotPath] {
            providerSettings.setDiscoveredModels(
                        providerSettings.activeProfileId(),
                        {QStringLiteral("deepseek-v4-flash"),
                         QStringLiteral("deepseek-v4-pro")});
            QObject *selector = view.rootObject()
                    ? view.rootObject()->findChild<QObject *>(QStringLiteral("collapsedModelSelector"))
                    : nullptr;
            if (!selector || !QMetaObject::invokeMethod(selector, "openPickerForSmoke")) {
                qCritical() << "MODEL_PICKER_SMOKE_ERROR selector unavailable";
                app.exit(10);
                return;
            }
            QTimer::singleShot(500, &app, [&app, selector, screenshotPath] {
                const QString pickerPath = QFileInfo(screenshotPath).dir()
                        .filePath(QStringLiteral("model_picker_smoke.png"));
                QQuickWindow *pickerWindow = selector->findChild<QQuickWindow *>(
                            QStringLiteral("modelPickerPopup"));
                QQuickItem *pickerContent = pickerWindow ? pickerWindow->contentItem() : nullptr;
                const QSharedPointer<QQuickItemGrabResult> result = pickerContent
                        ? pickerContent->grabToImage(pickerWindow->size())
                        : QSharedPointer<QQuickItemGrabResult>();
                if (result.isNull()) {
                    qCritical() << "MODEL_PICKER_SMOKE_ERROR window content unavailable";
                    app.exit(10);
                    return;
                }
                QObject::connect(result.data(), &QQuickItemGrabResult::ready, &app,
                                 [result, pickerPath, selector, &app] {
                    if (result->image().isNull() || !result->image().save(pickerPath, "PNG")) {
                        qCritical() << "MODEL_PICKER_SMOKE_ERROR save failed";
                        app.exit(10);
                        return;
                    }
                    writeStandardOutput("SMOKE_MODEL_PICKER_SCREENSHOT=%s\n",
                                        qPrintable(pickerPath));
                    QMetaObject::invokeMethod(selector, "closePickerForSmoke");
                    QTimer::singleShot(60, &app, [selector, &app] {
                        QMetaObject::invokeMethod(selector, "openPickerForSmoke");
                        QTimer::singleShot(80, &app, [selector] {
                            const bool reopened = selector->property("pickerVisible").toBool();
                            writeStandardOutput("SMOKE_MODEL_PICKER_REOPEN=%d\n",
                                                reopened ? 1 : 0);
                            QMetaObject::invokeMethod(selector, "closePickerForSmoke");
                        });
                    });
                });
            });
        });
        QTimer::singleShot(1450, &app, [&app, &view, screenshotPath] {
            QObject *rootObject = view.rootObject();
            if (!rootObject || !QMetaObject::invokeMethod(rootObject, "openHistoryForSmoke")) {
                qCritical() << "HISTORY_SMOKE_ERROR root method unavailable";
                app.exit(11);
                return;
            }
            QTimer::singleShot(300, &app, [&app, &view, rootObject, screenshotPath] {
                const QString historyPath = QFileInfo(screenshotPath).dir()
                        .filePath(QStringLiteral("history_smoke.png"));
                QQuickItem *rootItem = view.rootObject();
                const QSharedPointer<QQuickItemGrabResult> result = rootItem
                        ? rootItem->grabToImage(view.size())
                        : QSharedPointer<QQuickItemGrabResult>();
                if (result.isNull()) {
                    qCritical() << "HISTORY_SMOKE_ERROR grab unavailable";
                    app.exit(11);
                    return;
                }
                QObject::connect(result.data(), &QQuickItemGrabResult::ready, &app,
                                 [result, historyPath, rootObject, &app, &view, screenshotPath] {
                    if (result->image().isNull() || !result->image().save(historyPath, "PNG")) {
                        qCritical() << "HISTORY_SMOKE_ERROR save failed";
                        app.exit(11);
                        return;
                    }
                    writeStandardOutput("SMOKE_HISTORY_SCREENSHOT=%s\n",
                                        qPrintable(historyPath));
                    if (!QMetaObject::invokeMethod(rootObject, "openDeleteConfirmForSmoke")) {
                        qCritical() << "DELETE_CONFIRM_SMOKE_ERROR method unavailable";
                        app.exit(12);
                        return;
                    }
                    QTimer::singleShot(250, &app, [&app, &view, rootObject, screenshotPath] {
                        const QString deletePath = QFileInfo(screenshotPath).dir()
                                .filePath(QStringLiteral("delete_confirm_smoke.png"));
                        QQuickItem *content = view.contentItem();
                        const QSharedPointer<QQuickItemGrabResult> deleteResult = content
                                ? content->grabToImage(view.size())
                                : QSharedPointer<QQuickItemGrabResult>();
                        if (deleteResult.isNull()) {
                            qCritical() << "DELETE_CONFIRM_SMOKE_ERROR grab unavailable";
                            app.exit(12);
                            return;
                        }
                        QObject::connect(deleteResult.data(), &QQuickItemGrabResult::ready, &app,
                                         [deleteResult, deletePath, rootObject, &app] {
                            if (deleteResult->image().isNull()
                                    || !deleteResult->image().save(deletePath, "PNG")) {
                                qCritical() << "DELETE_CONFIRM_SMOKE_ERROR save failed";
                                app.exit(12);
                                return;
                            }
                            writeStandardOutput("SMOKE_DELETE_CONFIRM_SCREENSHOT=%s\n",
                                                qPrintable(deletePath));
                            QMetaObject::invokeMethod(rootObject, "closeDeleteConfirmForSmoke");
                        });
                    });
                });
            });
        });
        QTimer::singleShot(2250, &app, [&app, &view, screenshotPath] {
            QObject *rootObject = view.rootObject();
            if (!rootObject || !QMetaObject::invokeMethod(rootObject, "showTooltipForSmoke")) {
                qCritical() << "TOOLTIP_SMOKE_ERROR method unavailable";
                app.exit(13);
                return;
            }
            QTimer::singleShot(180, &app, [&app, &view, rootObject, screenshotPath] {
                const QString tooltipPath = QFileInfo(screenshotPath).dir()
                        .filePath(QStringLiteral("tooltip_smoke.png"));
                QQuickItem *content = view.contentItem();
                const QSharedPointer<QQuickItemGrabResult> tooltipResult = content
                        ? content->grabToImage(view.size())
                        : QSharedPointer<QQuickItemGrabResult>();
                if (tooltipResult.isNull()) {
                    qCritical() << "TOOLTIP_SMOKE_ERROR grab unavailable";
                    app.exit(13);
                    return;
                }
                QObject::connect(tooltipResult.data(), &QQuickItemGrabResult::ready, &app,
                                 [tooltipResult, tooltipPath, rootObject, &app] {
                    if (tooltipResult->image().isNull()
                            || !tooltipResult->image().save(tooltipPath, "PNG")) {
                        qCritical() << "TOOLTIP_SMOKE_ERROR save failed";
                        app.exit(13);
                        return;
                    }
                    writeStandardOutput("SMOKE_TOOLTIP_SCREENSHOT=%s\n",
                                        qPrintable(tooltipPath));
                    QMetaObject::invokeMethod(rootObject, "hideTooltipForSmoke");
                });
            });
        });
    } else if (startupAction == smartkey::StartupAction::ShowManualUi) {
        windowController.reveal();
        QTimer::singleShot(80, &app, [&view] {
            view.setFlags(Qt::Window | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
            view.setTitle(QStringLiteral("SmartKey AI UI Test"));
            view.show();
#ifdef Q_OS_WIN
            const HWND hwnd = reinterpret_cast<HWND>(view.winId());
            LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
            style &= ~WS_EX_TOOLWINDOW;
            style |= WS_EX_APPWINDOW;
            SetWindowLongPtrW(hwnd, GWL_EXSTYLE, style);
            SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER
                         | SWP_NOACTIVATE | SWP_FRAMECHANGED);
#endif
        });
    } else if (startupAction == smartkey::StartupAction::ShowSettings) {
        showSettings();
    } else if (startupAction == smartkey::StartupAction::HideToTray) {
        windowController.hideToTray();
    } else {
        windowController.reveal();
    }

    if (!isolatedUiTest && privacyConsent.consentGranted())
        scheduleModelRefresh();

    if (!smokeTest)
        return app.exec();

    QTimer::singleShot(3000, &app, [&app, &view, screenshotPath, focusChatInput] {
        if (view.status() == QQuickView::Error || !view.rootObject()) {
            qCritical() << "QML_COMPONENT_ERROR after event processing";
            app.exit(4);
            return;
        }
        focusChatInput();
        const QQuickItem *focusedItem = view.activeFocusItem();
        const QString focusedObjectName = focusedItem ? focusedItem->objectName() : QString();
        writeStandardOutput("SMOKE_FOCUSED_ITEM=%s\n", qPrintable(focusedObjectName));
        if (focusedObjectName != QStringLiteral("chatInput")) {
            qCritical().noquote() << "INPUT_FOCUS_ERROR expected chatInput got" << focusedObjectName;
            app.exit(8);
            return;
        }
        QDir().mkpath(QFileInfo(screenshotPath).absolutePath());
        const QImage windowImage = view.grabWindow();
        if (!windowImage.isNull() && windowImage.save(screenshotPath, "PNG")
                && QFileInfo(screenshotPath).size() > 0) {
            writeStandardOutput("SMOKE_CAPTURE_METHOD=grabWindow\n");
            writeStandardOutput("SMOKE_SCREENSHOT=%s\n", qPrintable(screenshotPath));
            writeStandardOutput("SMOKE_SCREENSHOT_SIZE=%lld\n",
                                static_cast<long long>(QFileInfo(screenshotPath).size()));
            std::fflush(stdout);
            app.exit(0);
            return;
        }

        QQuickItem *rootItem = view.rootObject();
        const QSharedPointer<QQuickItemGrabResult> result = rootItem
                ? rootItem->grabToImage(view.size()) : QSharedPointer<QQuickItemGrabResult>();
        if (result.isNull()) {
            writeStandardError("SMOKE_SCREENSHOT_FAILED grabWindow=null grabToImage=unavailable\n");
            std::fflush(stderr);
            app.exit(5);
            return;
        }
        QObject::connect(result.data(), &QQuickItemGrabResult::ready, &app,
                         [result, screenshotPath, &app] {
            const QImage image = result->image();
            if (image.isNull() || !image.save(screenshotPath, "PNG")
                    || QFileInfo(screenshotPath).size() <= 0) {
                writeStandardError("SMOKE_SCREENSHOT_FAILED grabToImage=null\n");
                std::fflush(stderr);
                app.exit(5);
                return;
            }
            writeStandardOutput("SMOKE_CAPTURE_METHOD=grabToImage_after_grabWindow_attempt\n");
            writeStandardOutput("SMOKE_SCREENSHOT=%s\n", qPrintable(screenshotPath));
            writeStandardOutput("SMOKE_SCREENSHOT_SIZE=%lld\n",
                                static_cast<long long>(QFileInfo(screenshotPath).size()));
            std::fflush(stdout);
            app.exit(0);
        });
    });
    return app.exec();
}
