#include <QGuiApplication>
#include <QKeyEvent>
#include <QRect>
#include <QWindow>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlError>
#include <QQmlContext>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>
#include <QVariantMap>

#include <cstdio>
#include "app/uipreferences.h"

class ProviderSettingsSmokeMock final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList availableModels READ availableModels NOTIFY modelDiscoveryChanged)
    Q_PROPERTY(QString activeModel READ activeModel NOTIFY activeSelectionChanged)
    Q_PROPERTY(QString activeReasoningMode READ activeReasoningMode NOTIFY activeSelectionChanged)
    Q_PROPERTY(QString activeProfileId READ activeProfileId CONSTANT)
    Q_PROPERTY(bool modelDiscoveryInProgress READ modelDiscoveryInProgress NOTIFY modelDiscoveryChanged)
    Q_PROPERTY(QString modelDiscoveryMessage READ modelDiscoveryMessage NOTIFY modelDiscoveryChanged)
public:
    explicit ProviderSettingsSmokeMock(QObject *parent = nullptr) : QObject(parent)
    {
        for (int index = 1; index <= 50; ++index)
            m_models.append(QStringLiteral("deepseek-model-%1").arg(index, 2, 10, QLatin1Char('0')));
        m_activeModel = m_models.first();
    }
    QStringList availableModels() const { return m_models; }
    QString activeModel() const { return m_activeModel; }
    QString activeReasoningMode() const { return m_reasoningMode; }
    QString activeProfileId() const { return QStringLiteral("smoke-profile"); }
    bool modelDiscoveryInProgress() const { return false; }
    QString modelDiscoveryMessage() const { return {}; }
    Q_INVOKABLE QVariantMap profile(const QString &) const
    {
        return {{QStringLiteral("supportsReasoning"), true},
                {QStringLiteral("supportsSearch"), true}};
    }
    Q_INVOKABLE bool refreshModels() { emit modelDiscoveryChanged(); return true; }
    Q_INVOKABLE bool setActiveModel(const QString &model)
    {
        if (!m_models.contains(model)) return false;
        m_activeModel = model; emit activeSelectionChanged(); return true;
    }
    Q_INVOKABLE bool setActiveReasoningMode(const QString &mode)
    {
        m_reasoningMode = mode; emit activeSelectionChanged(); return true;
    }
signals:
    void modelDiscoveryChanged();
    void activeSelectionChanged();
private:
    QStringList m_models;
    QString m_activeModel;
    QString m_reasoningMode = QStringLiteral("high");
};

class WindowGeometrySmokeMock final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(qreal scale READ scale CONSTANT)
public:
    using QObject::QObject;
    qreal scale() const { return 1.0; }
    Q_INVOKABLE QRectF availableGeometry() const { return QRectF(0, 0, 800, 600); }
};

int main(int argc, char **argv)
{
    QTemporaryDir isolatedData(
                QDir::temp().filePath(QStringLiteral("SmartKeyAI-qml-smoke-XXXXXX")));
    if (!isolatedData.isValid()) {
        std::fprintf(stderr, "QML_SMOKE_ISOLATION_ERROR temporary root unavailable\n");
        return 1;
    }
    qputenv("QML_DISABLE_DISK_CACHE", QByteArrayLiteral("1"));
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                       isolatedData.filePath(QStringLiteral("config")));
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope,
                       isolatedData.filePath(QStringLiteral("config")));
    QGuiApplication app(argc, argv);
    const bool selectorOnly = app.arguments().contains(
                QStringLiteral("--model-selector-only"));
    const bool accessibilityOnly = app.arguments().contains(
                QStringLiteral("--accessibility-only"));
    const bool targetedOnly = selectorOnly || accessibilityOnly;
    app.setApplicationName(QStringLiteral("SmartKey AI QML Smoke"));
    app.setOrganizationName(QStringLiteral("SmartKeyAI-Test"));
    std::fprintf(stdout, "QML_SMOKE_DATA_ROOT=%s\n", qPrintable(isolatedData.path()));
    QQmlEngine engine;
    UiPreferences uiPreferences;
    uiPreferences.setEngine(&engine);
    ProviderSettingsSmokeMock providerSettings;
    WindowGeometrySmokeMock windowGeometry;
    engine.rootContext()->setContextProperty(QStringLiteral("providerSettings"), &providerSettings);
    engine.rootContext()->setContextProperty(QStringLiteral("window"), &windowGeometry);
    engine.rootContext()->setContextProperty(QStringLiteral("uiPreferences"), &uiPreferences);
    engine.rootContext()->setContextProperty(
                QStringLiteral("fontManager"),
                QVariantMap{{QStringLiteral("item"),
                             QVariantMap{{QStringLiteral("uiFontFamily"),
                                          QStringLiteral("Segoe UI")}}}});
    int warningCount = 0;
    QObject::connect(&engine, &QQmlEngine::warnings, &app,
                     [&warningCount](const QList<QQmlError> &warnings) {
        warningCount += warnings.size();
        for (const QQmlError &warning : warnings)
            std::fprintf(stderr, "QML_WARNING %s\n", qPrintable(warning.toString()));
    });

    QObject *root = nullptr;
    if (!targetedOnly) {
        QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/markdown_smoke.qml")));
        if (component.status() == QQmlComponent::Error) {
            for (const QQmlError &error : component.errors())
                std::fprintf(stderr, "QML_ERROR %s\n", qPrintable(error.toString()));
            return 2;
        }
        root = component.create();
        if (!root) {
            for (const QQmlError &error : component.errors())
                std::fprintf(stderr, "QML_ERROR %s\n", qPrintable(error.toString()));
            return 3;
        }
    }

    QQmlComponent selectorComponent(&engine,
                                    QUrl(QStringLiteral("qrc:/model_selector_smoke.qml")));
    if (selectorComponent.status() == QQmlComponent::Error) {
        for (const QQmlError &error : selectorComponent.errors())
            std::fprintf(stderr, "MODEL_SELECTOR_QML_ERROR %s\n",
                         qPrintable(error.toString()));
        delete root;
        return 5;
    }
    QObject *selectorRoot = selectorComponent.create();
    if (!selectorRoot) {
        for (const QQmlError &error : selectorComponent.errors())
            std::fprintf(stderr, "MODEL_SELECTOR_QML_ERROR %s\n",
                         qPrintable(error.toString()));
        delete root;
        return 6;
    }

    QQmlComponent accessibilityComponent(
                &engine, QUrl(QStringLiteral("qrc:/accessibility_smoke.qml")));
    if (accessibilityComponent.status() == QQmlComponent::Error) {
        for (const QQmlError &error : accessibilityComponent.errors())
            std::fprintf(stderr, "ACCESSIBILITY_QML_ERROR %s\n",
                         qPrintable(error.toString()));
        delete selectorRoot;
        delete root;
        return 7;
    }
    QObject *accessibilityRoot = accessibilityComponent.create();
    if (!accessibilityRoot) {
        for (const QQmlError &error : accessibilityComponent.errors())
            std::fprintf(stderr, "ACCESSIBILITY_QML_ERROR %s\n",
                         qPrintable(error.toString()));
        delete selectorRoot;
        delete root;
        return 8;
    }

    QQmlComponent themeComponent(&engine, QUrl(QStringLiteral("qrc:/theme_i18n_smoke.qml")));
    if (themeComponent.status() == QQmlComponent::Error) {
        for (const QQmlError &error : themeComponent.errors())
            std::fprintf(stderr, "THEME_I18N_QML_ERROR %s\n", qPrintable(error.toString()));
        delete accessibilityRoot; delete selectorRoot; delete root; return 9;
    }
    QObject *themeRoot = themeComponent.create();
    if (!themeRoot) {
        for (const QQmlError &error : themeComponent.errors())
            std::fprintf(stderr, "THEME_I18N_QML_ERROR %s\n", qPrintable(error.toString()));
        delete accessibilityRoot; delete selectorRoot; delete root; return 10;
    }

    QTimer::singleShot(20, &app, [accessibilityRoot]() {
        QWindow *accessibilityWindow = qobject_cast<QWindow *>(accessibilityRoot);
        QObject *first = accessibilityRoot->findChild<QObject *>(
                    QStringLiteral("firstTabItem"));
        QObject *second = accessibilityRoot->findChild<QObject *>(
                    QStringLiteral("secondTabItem"));
        if (!accessibilityWindow || !first || !second
                || !first->property("activeFocus").toBool())
            return;
        accessibilityRoot->setProperty(
                    "focusContractPassed", first->property("focusRingVisible").toBool());
        QKeyEvent press(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier);
        QCoreApplication::sendEvent(accessibilityWindow, &press);
        QKeyEvent release(QEvent::KeyRelease, Qt::Key_Tab, Qt::NoModifier);
        QCoreApplication::sendEvent(accessibilityWindow, &release);
        accessibilityRoot->setProperty(
                    "tabOrderPassed", second->property("activeFocus").toBool());
    });

    QTimer::singleShot(2000, &app, &QCoreApplication::quit);
    app.exec();
    const bool passed = targetedOnly || root->property("passed").toBool();
    const bool selectorPassed = selectorRoot->property("passed").toBool();
    const QString selectorFailure = selectorRoot->property("failure").toString();
    QFile settingsSource(QStringLiteral(":/SettingsPageSource.qml"));
    const bool settingsReadable = settingsSource.open(QIODevice::ReadOnly);
    const QByteArray settingsQml = settingsReadable ? settingsSource.readAll() : QByteArray();
    const bool settingsUxContract = targetedOnly || (settingsQml.contains("objectName: \"settingsCtaBar\"")
            && settingsQml.contains("anchors.bottom: parent.bottom")
            && settingsQml.contains("ctaBarVisibleAtMinimumHeight")
            && settingsQml.contains("objectName: \"unsavedChangesConfirmation\"")
            && settingsQml.contains("objectName: \"providerDestructiveConfirmation\"")
            && settingsQml.contains("objectName: \"testConnectionButton\"")
            && settingsQml.contains("enabled: !root.testingConnection")
            && settingsQml.contains("if (!succeeded)")
            && settingsQml.contains("root.requestProfileSelection(index)"));
    QFile mainSource(QStringLiteral(":/MainViewSource.qml"));
    const bool mainReadable = mainSource.open(QIODevice::ReadOnly);
    const QByteArray mainQml = mainReadable ? mainSource.readAll() : QByteArray();
    const bool accessibilityRuntime = accessibilityRoot->property("passed").toBool();
    const bool lightThemeOnlyContract = settingsQml.contains("source: \"qrc:/Theme/LightTheme.qml\"")
            && mainQml.contains("source: \"qrc:/Theme/LightTheme.qml\"")
            && !settingsQml.contains("themePicker")
            && !settingsQml.contains("Theme/DarkTheme.qml")
            && !settingsQml.contains("Theme/HighContrastTheme.qml")
            && !mainQml.contains("Theme/DarkTheme.qml")
            && !mainQml.contains("Theme/HighContrastTheme.qml");
    const bool themeI18nPassed = themeRoot->property("passed").toBool()
            && lightThemeOnlyContract;
    const QString themeI18nFailure = themeRoot->property("failure").toString();
    const QString accessibilityFailure = accessibilityRoot->property("failure").toString();
    const bool accessibilitySourceContract = settingsQml.contains("HotkeyRecorder {")
            && settingsQml.contains("readonly property bool hotkeyRecording")
            && mainQml.contains("textEdit.inputMethodComposing")
            && mainQml.contains("textEdit.text.trim().length > 0")
            && mainQml.contains("enabled: dialog_manager.responseGenerated")
            && mainQml.contains("Accessible.name: qsTr(\"新对话\")")
            && mainQml.contains("activeFocusOnTab: true")
            && mainQml.contains("compactToolbarHeight: Math.max")
            && mainQml.contains("visible: dialog_manager.responseGenerated")
            && !mainQml.contains("color: theme.item.shadowColor");
    const bool accessibilityPassed = accessibilityRuntime
            && accessibilitySourceContract;
    const bool windowResizeContract = mainQml.contains("objectName: \"mainWindowResizeLayer\"")
            && mainQml.contains("objectName: \"mainWindowBottomRightResizeHandle\"")
            && mainQml.contains("window.beginSystemResize(edges)")
            && mainQml.contains("window.savedMainWindowSize()")
            && mainQml.contains("window.saveMainWindowSize(preferredMainWidth")
            && mainQml.contains("Qt.LeftEdge | Qt.TopEdge")
            && mainQml.contains("Qt.RightEdge | Qt.TopEdge")
            && mainQml.contains("Qt.LeftEdge | Qt.BottomEdge")
            && mainQml.contains("Qt.RightEdge | Qt.BottomEdge")
            && mainQml.contains("root.width - root.historyWidthOffset()")
            && mainQml.contains("minimumMainWidth: 480")
            && mainQml.contains("minimumExpandedHeight: 440");
    const bool escapeContractPassed = accessibilityRoot->property("escapeContractPassed").toBool();
    const bool tabContractPassed = accessibilityRoot->property("tabContractPassed").toBool();
    const bool tabOrderPassed = accessibilityRoot->property("tabOrderPassed").toBool();
    const bool namesContractPassed = accessibilityRoot->property("namesContractPassed").toBool();
    const bool focusContractPassed = accessibilityRoot->property("focusContractPassed").toBool();
    const bool contrastContractPassed = accessibilityRoot->property("contrastContractPassed").toBool();
    QFile historySource(QStringLiteral(":/ChatHistorySource.qml"));
    const bool historyReadable = historySource.open(QIODevice::ReadOnly);
    const QByteArray historyQml = historyReadable ? historySource.readAll() : QByteArray();
    const bool historyManagementContract = targetedOnly || (
            historyQml.contains("objectName: \"conversationExportButton\"")
            && historyQml.contains("exportChatById")
            && historyQml.contains("\"markdown\"")
            && !historyQml.contains("objectName: \"historySearchField\"")
            && !historyQml.contains("objectName: \"historyDateFilter\"")
            && !historyQml.contains("objectName: \"historyModelFilter\"")
            && !historyQml.contains("exportFilteredHistory")
            && historyQml.contains("historySection")
            && historyQml.contains("restoreChatById")
            && historyQml.contains("purgeDeletedChatById"));
    QFile dataManagementSource(QStringLiteral(":/DataManagementPanelSource.qml"));
    const bool dataManagementReadable = dataManagementSource.open(QIODevice::ReadOnly);
    const QByteArray dataManagementQml = dataManagementReadable
            ? dataManagementSource.readAll() : QByteArray();
    const bool dataManagementContract = targetedOnly || (
            settingsQml.contains("objectName: \"dataPrivacyTab\"")
            && settingsQml.contains("objectName: \"dataManagementLoader\"")
            && dataManagementQml.contains("objectName: \"dataDestructiveConfirmation\"")
            && dataManagementQml.contains("objectName: \"historyPersistenceSwitch\"")
            && dataManagementQml.contains("objectName: \"historyRetentionSelector\"")
            && dataManagementQml.contains("clearLogs()")
            && dataManagementQml.contains("clearAllChats()")
            && dataManagementQml.contains("clearAllCredentials()")
            && !dataManagementQml.contains("exportAllChats")
            && !dataManagementQml.contains("exportDiagnostics"));
    if (!passed && root)
        std::fprintf(stderr, "MARKDOWN_RENDERED_HTML=%s\n",
                     qPrintable(root->property("renderedHtml").toString()));
    delete themeRoot;
    delete accessibilityRoot;
    delete selectorRoot;
    delete root;
    std::fprintf(stdout, "QML_UI_SMOKE=%s MODEL_SELECTOR_SMOKE=%s SETTINGS_UX_CONTRACT=%s ACCESSIBILITY_SMOKE=%s WINDOW_RESIZE_CONTRACT=%s THEME_I18N_SMOKE=%s HISTORY_MANAGEMENT_CONTRACT=%s DATA_MANAGEMENT_CONTRACT=%s WARNINGS=%d\n",
                  passed ? "PASS" : "FAIL", selectorPassed ? "PASS" : "FAIL",
                  settingsUxContract ? "PASS" : "FAIL",
                  accessibilityPassed ? "PASS" : "FAIL",
                  windowResizeContract ? "PASS" : "FAIL",
                  themeI18nPassed ? "PASS" : "FAIL",
                 historyManagementContract ? "PASS" : "FAIL",
                 dataManagementContract ? "PASS" : "FAIL", warningCount);
    if (!selectorPassed)
        std::fprintf(stderr, "MODEL_SELECTOR_FAILURE=%s\n", qPrintable(selectorFailure));
    if (!accessibilityPassed)
        std::fprintf(stderr, "ACCESSIBILITY_FAILURE=%s SOURCE_CONTRACT=%d ESCAPE=%d TAB_CONTRACT=%d TAB_ORDER=%d NAMES=%d FOCUS=%d CONTRAST=%d\n",
                     qPrintable(accessibilityFailure), accessibilitySourceContract ? 1 : 0,
                     escapeContractPassed ? 1 : 0, tabContractPassed ? 1 : 0,
                     tabOrderPassed ? 1 : 0, namesContractPassed ? 1 : 0,
                     focusContractPassed ? 1 : 0, contrastContractPassed ? 1 : 0);
    if (!themeI18nPassed)
        std::fprintf(stderr, "THEME_I18N_FAILURE=%s\n", qPrintable(themeI18nFailure));
    return passed && selectorPassed && settingsUxContract
            && accessibilityPassed && windowResizeContract
            && themeI18nPassed && historyManagementContract
            && dataManagementContract && warningCount == 0 ? 0 : 4;
}

#include "qml_ui_smoke.moc"
