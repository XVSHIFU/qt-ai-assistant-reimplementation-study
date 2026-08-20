#include "autostartservice.h"
#include "hotkeyservice.h"
#include "lifecyclecoordinator.h"
#include "rapooaikeyadapter.h"
#include "singleinstanceservice.h"
#include "windowcontroller.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QProcess>
#include <QQuickView>
#include <QScreen>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTextStream>
#include <QTimer>
#include <QUuid>

#include <atomic>
#include <thread>

class FakeRapooDevice final : public QObject
{
    Q_OBJECT
signals:
    void reviceDeviceAIKeyPresssed();
};

class PlatformServicesTest final : public QObject
{
    Q_OBJECT

private slots:
    void windowControllerEnforcesToolWindowContract();
    void singleInstanceForwardsActivation();
    void singleInstanceIdentityIsInstallPathIndependent();
    void singleInstanceIdentityIsPerUser();
    void lifecycleTrayExitIsOrderedAndIdempotent();
    void lifecycleQmlExitIsOrderedAndIdempotent();
    void lifecycleAboutToQuitDoesNotRequit();
    void rapooAdapterDegradesAndBridges();
    void autoStartCommandIsQuotedAndBackgrounded();
    void hotkeyRejectsBareTextKeys();
    void hotkeyConflictKeepsOldBinding();
    void hotkeyToggleShowsThenHides();
    void firstRevealIsCenteredAboveWorkAreaBottom();
    void geometryClampPreservesNegativeScreenCoordinates();
    void geometryClampFitsSmallWorkArea();
};

void PlatformServicesTest::windowControllerEnforcesToolWindowContract()
{
    QQuickView view;
    view.resize(576, 720);
    WindowController controller(&view);

    QCOMPARE(static_cast<int>(view.flags() & Qt::WindowType_Mask),
             static_cast<int>(Qt::Tool));
    QVERIFY(view.flags().testFlag(Qt::FramelessWindowHint));
    QVERIFY(view.flags().testFlag(Qt::NoDropShadowWindowHint));
    QCOMPARE(controller.width(), 576.0);
    QCOMPARE(controller.height(), 720.0);

    controller.setXandY(10, 20, 600, 700);
    const QRect expected = WindowController::clampedGeometry(
                QRect(10, 20, 600, 700), view.screen()->availableGeometry());
    QCOMPARE(controller.x(), qreal(expected.x()));
    QCOMPARE(controller.y(), qreal(expected.y()));
    QCOMPARE(controller.width(), qreal(expected.width()));
    QCOMPARE(controller.height(), qreal(expected.height()));
}

void PlatformServicesTest::singleInstanceForwardsActivation()
{
    const QString serverName = QStringLiteral("smartkey-test-%1")
            .arg(QUuid::createUuid().toString(QUuid::Id128));
    SingleInstanceService primary(serverName);
    QCOMPARE(primary.acquire(), SingleInstanceService::Primary);
    QSignalSpy activationSpy(&primary, &SingleInstanceService::activationRequested);

    std::atomic<int> secondaryResult{-1};
    std::thread secondaryThread([&] {
        SingleInstanceService secondary(serverName);
        secondaryResult.store(static_cast<int>(secondary.acquire()));
    });
    QTRY_COMPARE_WITH_TIMEOUT(activationSpy.count(), 1, 2000);
    QTRY_VERIFY_WITH_TIMEOUT(secondaryResult.load() >= 0, 2000);
    secondaryThread.join();
    QCOMPARE(secondaryResult.load(),
             static_cast<int>(SingleInstanceService::SecondaryNotified));
}

void PlatformServicesTest::singleInstanceIdentityIsInstallPathIndependent()
{
    QTemporaryDir temporaryRoot;
    QVERIFY(temporaryRoot.isValid());

    const QString executable = QCoreApplication::applicationFilePath();
    const QString executableName = QFileInfo(executable).fileName();
    const QString firstDirectory = temporaryRoot.filePath(QStringLiteral("install-a"));
    const QString secondDirectory = temporaryRoot.filePath(QStringLiteral("install-b"));
    QVERIFY(QDir().mkpath(firstDirectory));
    QVERIFY(QDir().mkpath(secondDirectory));

    const QString firstExecutable = QDir(firstDirectory).filePath(executableName);
    const QString secondExecutable = QDir(secondDirectory).filePath(executableName);
    QVERIFY(QFile::copy(executable, firstExecutable));
    QVERIFY(QFile::copy(executable, secondExecutable));

    const QString fakeUserIdentity = QStringLiteral("test-user-%1")
            .arg(QUuid::createUuid().toString(QUuid::Id128));
    const QString primaryProbePath = temporaryRoot.filePath(QStringLiteral("primary.txt"));
    const QString secondaryProbePath = temporaryRoot.filePath(QStringLiteral("secondary.txt"));
    const QStringList primaryArguments{QStringLiteral("--single-instance-probe"),
                                       fakeUserIdentity, primaryProbePath};
    const QStringList secondaryArguments{QStringLiteral("--single-instance-probe"),
                                         fakeUserIdentity, secondaryProbePath};
    const auto readProbe = [](const QString &path) {
        QFile file(path);
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    };

    qint64 primaryPid = 0;
    QVERIFY(QProcess::startDetached(firstExecutable, primaryArguments,
                                    firstDirectory, &primaryPid));
    QVERIFY(primaryPid > 0);
    QTRY_VERIFY_WITH_TIMEOUT(QFileInfo(primaryProbePath).size() > 0, 10000);
    const QByteArray primaryLine = readProbe(primaryProbePath).split('\n').value(0).trimmed();
    QVERIFY2(primaryLine.startsWith("PRIMARY "), primaryLine.constData());

    qint64 secondaryPid = 0;
    QVERIFY(QProcess::startDetached(secondExecutable, secondaryArguments,
                                    secondDirectory, &secondaryPid));
    QVERIFY(secondaryPid > 0);
    QTRY_VERIFY_WITH_TIMEOUT(QFileInfo(secondaryProbePath).size() > 0, 10000);
    const QByteArray secondaryOutput = readProbe(secondaryProbePath).trimmed();
    QVERIFY2(secondaryOutput.startsWith("SECONDARY "), secondaryOutput.constData());
    QCOMPARE(secondaryOutput.mid(sizeof("SECONDARY ") - 1),
             primaryLine.mid(sizeof("PRIMARY ") - 1));

    QTRY_VERIFY_WITH_TIMEOUT(readProbe(primaryProbePath).contains("ACTIVATED"), 10000);
}

void PlatformServicesTest::singleInstanceIdentityIsPerUser()
{
    const QString firstIdentity = QStringLiteral("S-1-5-21-test-1001");
    const QString secondIdentity = QStringLiteral("S-1-5-21-test-1002");
    const QString firstServer =
            SingleInstanceService::serverNameForUserIdentity(firstIdentity);
    const QString firstServerAgain =
            SingleInstanceService::serverNameForUserIdentity(firstIdentity);
    const QString secondServer =
            SingleInstanceService::serverNameForUserIdentity(secondIdentity);

    QCOMPARE(firstServer, firstServerAgain);
    QVERIFY(!firstServer.contains(QCoreApplication::applicationFilePath(),
                                  Qt::CaseInsensitive));
    QVERIFY(firstServer != secondServer);

    SingleInstanceService firstUser(firstServer);
    SingleInstanceService secondUser(secondServer);
    QCOMPARE(firstUser.acquire(), SingleInstanceService::Primary);
    QCOMPARE(secondUser.acquire(), SingleInstanceService::Primary);
}

static smartkey::LifecycleCoordinator::Hooks lifecycleHooksForTrace(QStringList *trace)
{
    smartkey::LifecycleCoordinator::Hooks hooks;
    hooks.stopAcceptingRequests = [trace] { trace->append(QStringLiteral("stop")); };
    hooks.cancelRequests = [trace] { trace->append(QStringLiteral("cancel")); };
    hooks.flushAndPersist = [trace] { trace->append(QStringLiteral("flush")); };
    hooks.closeStorage = [trace] { trace->append(QStringLiteral("storage")); };
    hooks.unregisterIntegrations = [trace] { trace->append(QStringLiteral("unregister")); };
    hooks.quitApplication = [trace] { trace->append(QStringLiteral("quit")); };
    return hooks;
}

void PlatformServicesTest::lifecycleTrayExitIsOrderedAndIdempotent()
{
    QStringList trace;
    smartkey::LifecycleCoordinator::Hooks hooks = lifecycleHooksForTrace(&trace);
    smartkey::LifecycleCoordinator *coordinatorPointer = nullptr;
    const std::function<void()> cancelHook = hooks.cancelRequests;
    hooks.cancelRequests = [&] {
        cancelHook();
        // Simulate aboutToQuit/QML re-entry while shutdown is in progress.
        coordinatorPointer->requestQmlExit();
    };
    smartkey::LifecycleCoordinator coordinator(hooks);
    coordinatorPointer = &coordinator;
    int startedCount = 0;
    int completedCount = 0;
    connect(&coordinator, &smartkey::LifecycleCoordinator::shutdownStarted,
            this, [&](smartkey::LifecycleCoordinator::Trigger) { ++startedCount; });
    connect(&coordinator, &smartkey::LifecycleCoordinator::shutdownCompleted,
            this, [&](smartkey::LifecycleCoordinator::Trigger) { ++completedCount; });

    coordinator.requestTrayExit();
    coordinator.requestTrayExit();
    coordinator.requestSystemExit();

    QCOMPARE(trace, QStringList({QStringLiteral("stop"), QStringLiteral("cancel"),
                                 QStringLiteral("flush"), QStringLiteral("storage"),
                                 QStringLiteral("unregister"), QStringLiteral("quit")}));
    QCOMPARE(startedCount, 1);
    QCOMPARE(completedCount, 1);
    QVERIFY(coordinator.isShutdownCompleted());
    QCOMPARE(coordinator.trigger(), smartkey::LifecycleCoordinator::Trigger::TrayExit);
}

void PlatformServicesTest::lifecycleQmlExitIsOrderedAndIdempotent()
{
    QStringList trace;
    smartkey::LifecycleCoordinator coordinator(lifecycleHooksForTrace(&trace));
    coordinator.requestQmlExit();
    coordinator.requestSystemExit();

    QCOMPARE(trace, QStringList({QStringLiteral("stop"), QStringLiteral("cancel"),
                                 QStringLiteral("flush"), QStringLiteral("storage"),
                                 QStringLiteral("unregister"), QStringLiteral("quit")}));
    QCOMPARE(coordinator.trigger(), smartkey::LifecycleCoordinator::Trigger::QmlExit);
}

void PlatformServicesTest::lifecycleAboutToQuitDoesNotRequit()
{
    QStringList trace;
    smartkey::LifecycleCoordinator coordinator(lifecycleHooksForTrace(&trace));
    coordinator.requestSystemExit();
    coordinator.requestQmlExit();

    QCOMPARE(trace, QStringList({QStringLiteral("stop"), QStringLiteral("cancel"),
                                 QStringLiteral("flush"), QStringLiteral("storage"),
                                 QStringLiteral("unregister")}));
    QCOMPARE(coordinator.trigger(), smartkey::LifecycleCoordinator::Trigger::SystemExit);
    QVERIFY(coordinator.isShutdownCompleted());
}

void PlatformServicesTest::rapooAdapterDegradesAndBridges()
{
    RapooAiKeyAdapter adapter;
    QVERIFY(!adapter.isAvailable());
    QVERIFY(!adapter.start());

    FakeRapooDevice device;
    QVERIFY(adapter.attachDeviceSource(&device));
    QVERIFY(adapter.isAvailable());
    QSignalSpy activationSpy(&adapter, &RapooAiKeyAdapter::activated);
    emit device.reviceDeviceAIKeyPresssed();
    QCOMPARE(activationSpy.count(), 1);

    adapter.stop();
    QVERIFY(!adapter.isAvailable());
}

void PlatformServicesTest::autoStartCommandIsQuotedAndBackgrounded()
{
    AutoStartService service(QStringLiteral("SmartKeyAIPlatformTest"));
    const QString command = service.startupCommand();
    QVERIFY(command.startsWith(QLatin1Char('"')));
    QVERIFY(command.contains(QStringLiteral("\"--background\"")));
}

void PlatformServicesTest::hotkeyRejectsBareTextKeys()
{
    HotkeyService service;
    QString error;
    QVERIFY(!service.setBinding(0, 'A', &error));
    QVERIFY(error.contains(QStringLiteral("Ctrl")));
    QVERIFY(!service.isEnabled());
}

void PlatformServicesTest::hotkeyConflictKeepsOldBinding()
{
#ifndef Q_OS_WIN
    QSKIP("Windows RegisterHotKey contract test");
#else
    HotkeyService occupied;
    QString error;
    if (!occupied.registerDefault(&error))
        QSKIP(qPrintable(QStringLiteral("Default shortcut unavailable: %1").arg(error)));

    HotkeyService subject;
    const quint32 fallbackModifiers = HotkeyService::Control | HotkeyService::Alt;
    const quint32 fallbackKey = 0x79; // VK_F10
    if (!subject.setBinding(fallbackModifiers, fallbackKey, &error))
        QSKIP(qPrintable(QStringLiteral("Fallback shortcut unavailable: %1").arg(error)));

    QVERIFY(!subject.setBinding(HotkeyService::defaultModifiers(),
                                HotkeyService::defaultVirtualKey(),
                                &error));
    QVERIFY(subject.isEnabled());
    QCOMPARE(subject.modifiers(), fallbackModifiers);
    QCOMPARE(subject.virtualKey(), fallbackKey);
#endif
}

void PlatformServicesTest::hotkeyToggleShowsThenHides()
{
    QQuickView view;
    WindowController controller(&view);
    QVERIFY(!controller.isVisible());
    controller.toggleFromHotkey();
    QVERIFY(controller.isVisible());
    controller.toggleFromHotkey();
    QVERIFY(!controller.isVisible());
}

void PlatformServicesTest::firstRevealIsCenteredAboveWorkAreaBottom()
{
    QQuickView view;
    view.resize(576, 82);
    WindowController controller(&view);
    controller.reveal();
    QScreen *screen = QGuiApplication::screenAt(view.position());
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    QVERIFY(screen);
    const QRect available = screen->availableGeometry();
    const int expectedCenter = available.center().x();
    const int actualCenter = view.x() + view.width() / 2;
    QVERIFY(qAbs(expectedCenter - actualCenter) <= 2);
    QVERIFY(view.y() + view.height() <= available.bottom() + 1);
    QVERIFY(available.bottom() - (view.y() + view.height()) >= 12);
}

void PlatformServicesTest::geometryClampPreservesNegativeScreenCoordinates()
{
    const QRect leftMonitor(-1920, -180, 1920, 1040);
    const QRect requested(-2050, 700, 800, 500);
    const QRect result = WindowController::clampedGeometry(requested, leftMonitor);
    QCOMPARE(result, QRect(-1920, 360, 800, 500));
    QVERIFY(leftMonitor.contains(result));
}

void PlatformServicesTest::geometryClampFitsSmallWorkArea()
{
    const QRect smallWorkArea(120, 80, 360, 240);
    const QRect oversized(-100, -100, 700, 600);
    const QRect result = WindowController::clampedGeometry(oversized, smallWorkArea);
    QCOMPARE(result, smallWorkArea);
    QVERIFY(smallWorkArea.contains(result));
}

namespace {

int runSingleInstanceProbe(QCoreApplication &application, const QStringList &arguments)
{
    const int probeIndex = arguments.indexOf(QStringLiteral("--single-instance-probe"));
    if (probeIndex < 0 || probeIndex + 2 >= arguments.size())
        return 64;

    QFile outputFile(arguments.at(probeIndex + 2));
    if (!outputFile.open(QIODevice::WriteOnly | QIODevice::Text))
        return 65;

    const QString serverName = SingleInstanceService::serverNameForUserIdentity(
            arguments.at(probeIndex + 1));
    SingleInstanceService service(serverName);
    QString error;
    const SingleInstanceService::AcquireResult result = service.acquire(
            QStringLiteral("activate"), 2000, &error);
    QTextStream output(&outputFile);

    if (result == SingleInstanceService::SecondaryNotified) {
        output << "SECONDARY " << serverName << '\n';
        output.flush();
        return 0;
    }
    if (result != SingleInstanceService::Primary) {
        output << "FAILED " << error << '\n';
        output.flush();
        return 2;
    }

    output << "PRIMARY " << serverName << '\n';
    output.flush();
    bool activated = false;
    QObject::connect(&service, &SingleInstanceService::activationRequested,
                     &application, [&application, &activated] {
        activated = true;
        application.quit();
    });
    QTimer::singleShot(5000, &application, &QCoreApplication::quit);
    application.exec();
    if (activated) {
        output << "ACTIVATED\n";
        output.flush();
        return 0;
    }
    output << "TIMED_OUT\n";
    output.flush();
    return 3;
}

} // namespace

int main(int argc, char **argv)
{
    for (int index = 1; index < argc; ++index) {
        if (QString::fromLocal8Bit(argv[index]) == QStringLiteral("--single-instance-probe")) {
            QCoreApplication probeApplication(argc, argv);
            return runSingleInstanceProbe(probeApplication, probeApplication.arguments());
        }
    }

    QGuiApplication application(argc, argv);
    PlatformServicesTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "platform_services_test.moc"
