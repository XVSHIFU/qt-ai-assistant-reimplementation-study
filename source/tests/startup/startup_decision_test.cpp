#include "app/startupdecision.h"

#include <QDir>
#include <QTemporaryDir>
#include <QtTest>

class StartupDecisionTest final : public QObject
{
    Q_OBJECT

private slots:
    void launchMatrix_data();
    void launchMatrix();
    void smokeRootRequiresSafeTemporaryDirectory();
};

void StartupDecisionTest::launchMatrix_data()
{
    QTest::addColumn<bool>("smoke");
    QTest::addColumn<bool>("manual");
    QTest::addColumn<bool>("background");
    QTest::addColumn<bool>("tray");
    QTest::addColumn<bool>("configured");
    QTest::addColumn<bool>("onboarded");
    QTest::addColumn<int>("expected");

    using smartkey::StartupAction;
    QTest::newRow("smoke") << true << false << false << true << true << true
                            << static_cast<int>(StartupAction::RunSmoke);
    QTest::newRow("manual") << false << true << false << true << true << true
                             << static_cast<int>(StartupAction::ShowManualUi);
    QTest::newRow("first-run-normal") << false << false << false << true << false << false
                                      << static_cast<int>(StartupAction::ShowSettings);
    QTest::newRow("first-run-background") << false << false << true << true << false << false
                                          << static_cast<int>(StartupAction::ShowSettings);
    QTest::newRow("onboarding-incomplete") << false << false << false << true << true << false
                                           << static_cast<int>(StartupAction::ShowSettings);
    QTest::newRow("normal-visible") << false << false << false << true << true << true
                                    << static_cast<int>(StartupAction::RevealMain);
    QTest::newRow("background-tray") << false << false << true << true << true << true
                                     << static_cast<int>(StartupAction::HideToTray);
    QTest::newRow("background-without-tray") << false << false << true << false << true << true
                                             << static_cast<int>(StartupAction::RevealMain);
}

void StartupDecisionTest::launchMatrix()
{
    QFETCH(bool, smoke);
    QFETCH(bool, manual);
    QFETCH(bool, background);
    QFETCH(bool, tray);
    QFETCH(bool, configured);
    QFETCH(bool, onboarded);
    QFETCH(int, expected);
    QCOMPARE(static_cast<int>(smartkey::decideStartupAction(
                                  smoke, manual, background, tray, configured, onboarded)),
             expected);
}

void StartupDecisionTest::smokeRootRequiresSafeTemporaryDirectory()
{
    QTemporaryDir parent(QDir::temp().filePath(QStringLiteral("startup-root-test-XXXXXX")));
    QVERIFY(parent.isValid());
    const QString safe = parent.filePath(
                QStringLiteral("SmartKeyAI-smoke-startup-0123456789abcdef0123456789abcdef"));
    QVERIFY(QDir().mkpath(safe));
    QString canonical;
    QVERIFY(smartkey::isSafeSmokeDataRoot(safe, &canonical));
    QCOMPARE(canonical, QFileInfo(safe).canonicalFilePath());

    QVERIFY(!smartkey::isSafeSmokeDataRoot(parent.filePath(QStringLiteral("ordinary"))));
    QVERIFY(!smartkey::isSafeSmokeDataRoot(QDir::tempPath()));
    QVERIFY(!smartkey::isSafeSmokeDataRoot(QDir::currentPath()));
    QVERIFY(!smartkey::isSafeSmokeDataRoot(QStringLiteral("relative-smoke-root")));
}

QTEST_APPLESS_MAIN(StartupDecisionTest)
#include "startup_decision_test.moc"
