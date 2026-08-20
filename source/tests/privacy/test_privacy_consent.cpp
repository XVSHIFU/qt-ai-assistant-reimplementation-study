#include "privacy/privacy_consent_service.h"
#include "settings/provider_settings.h"

#ifndef PRIVACY_SERVICE_ONLY
#include "ai/aihttptransport.h"
#include "ai/openaicompatibleprovider.h"
#include "app/dialogmanager.h"
#include "storage/chat_storage.h"
#endif

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using namespace smartkey;
#ifndef PRIVACY_SERVICE_ONLY
using namespace smartkey::ai;
#endif

namespace {

QVariantMap noAuthProfile()
{
    return {
        {QStringLiteral("name"), QStringLiteral("Privacy test provider")},
        {QStringLiteral("baseUrl"), QStringLiteral("https://privacy-test.invalid")},
        {QStringLiteral("chatPath"), QStringLiteral("/v1/chat/completions")},
        {QStringLiteral("model"), QStringLiteral("privacy-test-model")},
        {QStringLiteral("providerType"), QStringLiteral("custom")},
        {QStringLiteral("authScheme"), QStringLiteral("none")},
        {QStringLiteral("authHeaderName"), QStringLiteral("Authorization")},
        {QStringLiteral("supportsStreaming"), true}
    };
}

#ifndef PRIVACY_SERVICE_ONLY
class CountingTransport final : public AiHttpTransport
{
public:
    using AiHttpTransport::AiHttpTransport;

    quint64 start(const AiHttpRequest &request) override
    {
        ++startCount;
        lastRequest = request;
        return static_cast<quint64>(startCount);
    }

    void cancel(quint64 requestId) override
    {
        Q_UNUSED(requestId)
    }

    int startCount = 0;
    AiHttpRequest lastRequest;
};
#endif

} // namespace

class PrivacyConsentTest final : public QObject
{
    Q_OBJECT

private slots:
    void firstLaunchStartsWithoutConsent();
    void acceptancePersistsRequiredMetadata();
    void policyUpgradeRequiresFreshConsent();
    void revocationBlocksAgain();
    void unconsentedNetworkActionsAreZero();
    void deletionEntryOnlyEmitsRequest();
};

void PrivacyConsentTest::firstLaunchStartsWithoutConsent()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    PrivacyConsentService consent(directory.filePath(QStringLiteral("privacy.ini")));
    QVERIFY(!consent.consentGranted());
    QVERIFY(consent.consentRequired());
    QVERIFY(consent.acceptedAt().isEmpty());
    QVERIFY(consent.policyText().contains(QStringLiteral("第三方 AI Provider")));
    QVERIFY(consent.policyText().contains(QStringLiteral("明文")));
    QVERIFY(!consent.policyText().contains(QStringLiteral("driverapi.rapoo.cn")));
}

void PrivacyConsentTest::acceptancePersistsRequiredMetadata()
{
    QTemporaryDir directory;
    const QString path = directory.filePath(QStringLiteral("privacy.ini"));
    PrivacyConsentService consent(path);
    QVERIFY(consent.accept(QStringLiteral("zh-CN")));
    QVERIFY(consent.consentGranted());
    QVERIFY(QDateTime::fromString(consent.acceptedAt(), Qt::ISODateWithMs).isValid());
    QCOMPARE(consent.acceptedLanguage(), QStringLiteral("zh-CN"));

    QSettings stored(path, QSettings::IniFormat);
    QVERIFY(stored.value(QStringLiteral("privacy/accepted")).toBool());
    QCOMPARE(stored.value(QStringLiteral("privacy/policyVersion")).toString(),
             PrivacyConsentService::currentPolicyVersion());
    QCOMPARE(stored.value(QStringLiteral("privacy/language")).toString(),
             QStringLiteral("zh-CN"));
    QVERIFY(QDateTime::fromString(
                stored.value(QStringLiteral("privacy/acceptedAt")).toString(),
                Qt::ISODateWithMs).isValid());

    PrivacyConsentService reloaded(path);
    QVERIFY(reloaded.consentGranted());
}

void PrivacyConsentTest::policyUpgradeRequiresFreshConsent()
{
    QTemporaryDir directory;
    const QString path = directory.filePath(QStringLiteral("privacy.ini"));
    PrivacyConsentService oldPolicy(path, QStringLiteral("policy-v1"));
    QVERIFY(oldPolicy.accept(QStringLiteral("zh-CN")));

    PrivacyConsentService upgraded(path, QStringLiteral("policy-v2"));
    QVERIFY(!upgraded.consentGranted());
    QVERIFY(upgraded.consentRequired());
    QCOMPARE(upgraded.policyVersion(), QStringLiteral("policy-v2"));
}

void PrivacyConsentTest::revocationBlocksAgain()
{
    QTemporaryDir directory;
    const QString path = directory.filePath(QStringLiteral("privacy.ini"));
    PrivacyConsentService consent(path);
    QVERIFY(consent.accept(QStringLiteral("zh-CN")));
    QVERIFY(consent.revoke());
    QVERIFY(!consent.consentGranted());
    QVERIFY(consent.acceptedAt().isEmpty());

    PrivacyConsentService reloaded(path);
    QVERIFY(!reloaded.consentGranted());
}

void PrivacyConsentTest::unconsentedNetworkActionsAreZero()
{
    QTemporaryDir directory;
    PrivacyConsentService consent(directory.filePath(QStringLiteral("privacy.ini")));
    ProviderSettings settings(directory.filePath(QStringLiteral("settings.ini")),
                              directory.filePath(QStringLiteral("credentials.ini")));
    settings.setPrivacyConsentService(&consent);
    const QString id = settings.saveProfile(noAuthProfile());
    QVERIFY(!id.isEmpty());

    QSignalSpy testRequests(&settings, &ProviderSettings::testConnectionRequested);
    QSignalSpy modelRequests(&settings, &ProviderSettings::modelDiscoveryRequested);
    QVERIFY(!settings.testConnection(id));
    QVERIFY(!settings.refreshModels());
    QCOMPARE(testRequests.count(), 0);
    QCOMPARE(modelRequests.count(), 0);

#ifndef PRIVACY_SERVICE_ONLY
    CountingTransport transport;
    OpenAiCompatibleProvider provider(&transport);
    ChatStorage storage(directory.filePath(QStringLiteral("chat.sqlite")));
    QVERIFY(storage.open());
    DialogManager manager(&settings, &provider, &storage);
    manager.setPrivacyConsentService(&consent);
    manager.sendMessage(QStringLiteral("must not leave this process"));
    QCOMPARE(transport.startCount, 0);
    QCOMPARE(manager.lastErrorCode(), QStringLiteral("privacy_consent_required"));
    QCOMPARE(qobject_cast<ChatConversationModel *>(manager.dialogModel())->rowCount(), 0);

    QVERIFY(consent.accept(QStringLiteral("zh-CN")));
    manager.sendMessage(QStringLiteral("allowed after explicit consent"));
    QCOMPARE(transport.startCount, 1);
#endif
}

void PrivacyConsentTest::deletionEntryOnlyEmitsRequest()
{
    QTemporaryDir directory;
    const QString sentinel = directory.filePath(QStringLiteral("chat.sqlite"));
    QFile file(sentinel);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("keep");
    file.close();

    PrivacyConsentService consent(directory.filePath(QStringLiteral("privacy.ini")));
    QSignalSpy requestSpy(&consent, &PrivacyConsentService::dataDeletionRequested);
    consent.requestDataDeletion();
    QCOMPARE(requestSpy.count(), 1);
    QVERIFY(QFileInfo::exists(sentinel));
}

QTEST_MAIN(PrivacyConsentTest)
#include "test_privacy_consent.moc"
