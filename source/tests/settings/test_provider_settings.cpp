#include "credential_store.h"
#include "jsonl_logger.h"
#include "provider_settings.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using namespace smartkey;

namespace {

QVariantMap validProfile()
{
    return {
        {QStringLiteral("name"), QStringLiteral("Local compatible API")},
        {QStringLiteral("baseUrl"), QStringLiteral("https://example.invalid/")},
        {QStringLiteral("chatPath"), QStringLiteral("v1/chat/completions")},
        {QStringLiteral("model"), QStringLiteral("test-model")},
        {QStringLiteral("providerType"), QStringLiteral("deepseek")},
        {QStringLiteral("authScheme"), QStringLiteral("bearer")},
        {QStringLiteral("authHeaderName"), QStringLiteral("Authorization")},
        {QStringLiteral("timeoutMs"), 90000},
        {QStringLiteral("contextLimit"), 4096},
        {QStringLiteral("outputLimit"), 512},
        {QStringLiteral("supportsStreaming"), true},
        {QStringLiteral("supportsReasoning"), true},
        {QStringLiteral("supportsSearch"), false}
        ,{QStringLiteral("thinkingEnabled"), true}
        ,{QStringLiteral("reasoningEffort"), QStringLiteral("high")}
    };
}

QByteArray readFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return file.readAll();
}

bool writeFile(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    return file.write(contents) == contents.size();
}

QVariantMap noAuthProfile()
{
    QVariantMap profile = validProfile();
    profile.insert(QStringLiteral("authScheme"), QStringLiteral("none"));
    return profile;
}

} // namespace

class ProviderSettingsTest : public QObject
{
    Q_OBJECT

private slots:
    void profileCrudAndPersistence();
    void profileContextLimitsAreBounded();
    void credentialUsesDpapiAndIsWriteOnlyToQml();
    void profileAndCredentialSaveIsAtomic();
    void endpointChangeRequiresFreshCredential();
    void credentialFailuresLeaveNoPartialProfile();
    void truncatedJsonIsPreserved();
    void unknownSchemaIsRejected();
    void legacySettingsMigrateOnExplicitSave();
    void atomicWriteFailureLeavesOriginalUnchanged();
    void validBackupIsRecoveredWithoutOverwritingPrimary();
    void startupAndTestConnectionContract();
    void modelSelectionAndReasoningContract();
    void capabilitiesComeFromBackendSchema();
    void loggerRedactsAndRotates();
};

void ProviderSettingsTest::profileCrudAndPersistence()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString settingsPath = directory.filePath(QStringLiteral("settings.ini"));
    const QString credentialsPath = directory.filePath(QStringLiteral("credentials.ini"));
    ProviderSettings settings(settingsPath, credentialsPath);

    QVariantMap input = validProfile();
    input.insert(QStringLiteral("apiKey"), QStringLiteral("must-never-be-persisted"));
    const QString id = settings.saveProfile(input);
    QVERIFY2(!id.isEmpty(), qPrintable(settings.persistenceMessage()));
    QCOMPARE(settings.activeProfileId(), id);
    QCOMPARE(settings.profiles().size(), 1);

    const QVariantMap stored = settings.profile(id);
    QCOMPARE(stored.value(QStringLiteral("baseUrl")).toString(), QStringLiteral("https://example.invalid"));
    QCOMPARE(stored.value(QStringLiteral("chatPath")).toString(), QStringLiteral("/v1/chat/completions"));
    QCOMPARE(stored.value(QStringLiteral("timeoutMs")).toInt(), 90000);
    QCOMPARE(stored.value(QStringLiteral("contextLimit")).toInt(), 4096);
    QCOMPARE(stored.value(QStringLiteral("outputLimit")).toInt(), 512);
    QVERIFY(stored.contains(QStringLiteral("hasCredential")));
    QVERIFY(stored.contains(QStringLiteral("supportsStreaming")));
    QVERIFY(stored.contains(QStringLiteral("supportsReasoning")));
    QVERIFY(stored.contains(QStringLiteral("supportsSearch")));
    QCOMPARE(stored.value(QStringLiteral("providerType")).toString(), QStringLiteral("deepseek"));
    QVERIFY(stored.value(QStringLiteral("thinkingEnabled")).toBool());
    QCOMPARE(stored.value(QStringLiteral("reasoningEffort")).toString(), QStringLiteral("high"));
    QVERIFY(!stored.contains(QStringLiteral("apiKey")));
    QVERIFY(!readFile(settingsPath).contains("must-never-be-persisted"));

    QVariantMap update = stored;
    update.insert(QStringLiteral("name"), QStringLiteral("Renamed"));
    QCOMPARE(settings.saveProfile(update), id);
    QCOMPARE(settings.profile(id).value(QStringLiteral("name")).toString(), QStringLiteral("Renamed"));

    ProviderSettings reloaded(settingsPath, credentialsPath);
    QCOMPARE(reloaded.profiles().size(), 1);
    QCOMPARE(reloaded.activeProfileId(), id);
    QVERIFY(reloaded.removeProfile(id));
    QVERIFY(reloaded.profiles().isEmpty());
}

void ProviderSettingsTest::profileContextLimitsAreBounded()
{
    QTemporaryDir directory;
    ProviderSettings settings(directory.filePath(QStringLiteral("settings.ini")),
                              directory.filePath(QStringLiteral("credentials.ini")));
    QVariantMap defaults = noAuthProfile();
    defaults.remove(QStringLiteral("contextLimit"));
    defaults.remove(QStringLiteral("outputLimit"));
    const QString defaultId = settings.saveProfile(defaults);
    QCOMPARE(settings.profile(defaultId).value(QStringLiteral("contextLimit")).toInt(), 32768);
    QCOMPARE(settings.profile(defaultId).value(QStringLiteral("outputLimit")).toInt(), 2048);

    QVariantMap bounded = noAuthProfile();
    bounded.remove(QStringLiteral("id"));
    bounded.insert(QStringLiteral("baseUrl"), QStringLiteral("https://bounded.invalid"));
    bounded.insert(QStringLiteral("contextLimit"), 100);
    bounded.insert(QStringLiteral("outputLimit"), 9999999);
    const QString boundedId = settings.saveProfile(bounded);
    const QVariantMap stored = settings.profile(boundedId);
    QCOMPARE(stored.value(QStringLiteral("contextLimit")).toInt(), 256);
    QCOMPARE(stored.value(QStringLiteral("outputLimit")).toInt(), 192);
}

void ProviderSettingsTest::capabilitiesComeFromBackendSchema()
{
    QTemporaryDir directory;
    ProviderSettings settings(directory.filePath(QStringLiteral("settings.ini")),
                              directory.filePath(QStringLiteral("credentials.ini")));

    QVariantMap deepSeek = noAuthProfile();
    deepSeek.insert(QStringLiteral("supportsStreaming"), false);
    deepSeek.insert(QStringLiteral("supportsSearch"), true);
    deepSeek.insert(QStringLiteral("searchRequestField"), QStringLiteral("enable_search"));
    const QString deepSeekId = settings.saveProfile(deepSeek);
    QVERIFY(!deepSeekId.isEmpty());
    const QVariantMap storedDeepSeek = settings.profile(deepSeekId);
    QCOMPARE(storedDeepSeek.value(QStringLiteral("capabilitySchema")).toString(),
             QStringLiteral("deepseek-chat-v1"));
    QVERIFY(storedDeepSeek.value(QStringLiteral("supportsStreaming")).toBool());
    QVERIFY(!storedDeepSeek.value(QStringLiteral("supportsSearch")).toBool());
    QVERIFY(storedDeepSeek.value(QStringLiteral("searchRequestField")).toString().isEmpty());

    QVariantMap nonStreaming = noAuthProfile();
    nonStreaming.remove(QStringLiteral("id"));
    nonStreaming.insert(QStringLiteral("providerType"), QStringLiteral("custom"));
    nonStreaming.insert(QStringLiteral("baseUrl"), QStringLiteral("https://nonstream.invalid"));
    nonStreaming.insert(QStringLiteral("capabilitySchema"),
                        QStringLiteral("openai-chat-nonstreaming-v1"));
    nonStreaming.insert(QStringLiteral("supportsStreaming"), true);
    nonStreaming.insert(QStringLiteral("supportsSearch"), true);
    const QString nonStreamingId = settings.saveProfile(nonStreaming);
    QVERIFY(!nonStreamingId.isEmpty());
    const QVariantMap storedNonStreaming = settings.profile(nonStreamingId);
    QVERIFY(!storedNonStreaming.value(QStringLiteral("supportsStreaming")).toBool());
    QVERIFY(!storedNonStreaming.value(QStringLiteral("supportsSearch")).toBool());
}

void ProviderSettingsTest::credentialUsesDpapiAndIsWriteOnlyToQml()
{
#ifndef Q_OS_WIN
    QSKIP("DPAPI test is Windows-only");
#endif
    QTemporaryDir directory;
    ProviderSettings settings(directory.filePath(QStringLiteral("settings.ini")),
                              directory.filePath(QStringLiteral("credentials.ini")));
    const QString id = settings.saveProfile(validProfile());
    const QString canary = QStringLiteral("sk-this-plaintext-must-not-reach-disk");
    QVERIFY(settings.setCredential(id, canary));
    QVERIFY(settings.hasCredential(id));
    QVERIFY(settings.configured());
    QVERIFY(readFile(settings.settingsFilePath()).contains("hasCredential\\\":true"));

    const QMetaObject &meta = ProviderSettings::staticMetaObject;
    QCOMPARE(meta.indexOfProperty("apiKey"), -1);
    QCOMPARE(meta.indexOfProperty("credential"), -1);
    QCOMPARE(meta.indexOfMethod("credential(QString)"), -1);

    QByteArray plaintext;
    QString error;
    QVERIFY2(settings.credential(id, &plaintext, &error), qPrintable(error));
    QCOMPARE(plaintext, canary.toUtf8());
    CredentialStore::clearSensitive(&plaintext);

    const QByteArray fileData = readFile(settings.credentialFilePath());
    QVERIFY(!fileData.isEmpty());
    QVERIFY(!fileData.contains(canary.toUtf8()));
    QVERIFY(settings.clearCredential(id));
    QVERIFY(!settings.hasCredential(id));
    QVERIFY(!settings.configured());
    QVERIFY(readFile(settings.settingsFilePath()).contains("hasCredential\\\":false"));
}

void ProviderSettingsTest::profileAndCredentialSaveIsAtomic()
{
#ifndef Q_OS_WIN
    QSKIP("DPAPI test is Windows-only");
#endif
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString settingsPath = directory.filePath(QStringLiteral("settings.ini"));
    const QString credentialsPath = directory.filePath(QStringLiteral("credentials.ini"));
    ProviderSettings settings(settingsPath, credentialsPath);

    const QString oldKey = QStringLiteral("atomic-old-test-key");
    const QString id = settings.saveProfileWithCredential(validProfile(), oldKey);
    QVERIFY(!id.isEmpty());
    QVERIFY(settings.configured());

    // Turn the settings file into an unwritable target after the initial commit.
    QVERIFY(QFile::remove(settingsPath));
    QVERIFY(QDir().mkpath(settingsPath));

    QVariantMap changed = settings.profile(id);
    changed.insert(QStringLiteral("baseUrl"), QStringLiteral("https://new-host.invalid"));
    QVERIFY(settings.saveProfileWithCredential(changed,
                                                QStringLiteral("atomic-new-test-key")).isEmpty());
    QCOMPARE(settings.profile(id).value(QStringLiteral("baseUrl")).toString(),
             QStringLiteral("https://example.invalid"));
    QVERIFY(settings.configured());

    QByteArray plaintext;
    QString error;
    QVERIFY2(settings.credential(id, &plaintext, &error), qPrintable(error));
    QCOMPARE(plaintext, oldKey.toUtf8());
    CredentialStore::clearSensitive(&plaintext);
}

void ProviderSettingsTest::endpointChangeRequiresFreshCredential()
{
#ifndef Q_OS_WIN
    QSKIP("DPAPI test is Windows-only");
#endif
    QTemporaryDir directory;
    ProviderSettings settings(directory.filePath(QStringLiteral("settings.ini")),
                              directory.filePath(QStringLiteral("credentials.ini")));
    const QString id = settings.saveProfileWithCredential(
                validProfile(), QStringLiteral("host-a-test-key"));
    QVERIFY(!id.isEmpty());

    QVariantMap changed = settings.profile(id);
    changed.insert(QStringLiteral("baseUrl"), QStringLiteral("https://host-b.invalid"));

    // The atomic UI API rejects the edit before either metadata or credential changes.
    QVERIFY(settings.saveProfileWithCredential(changed, QString()).isEmpty());
    QCOMPARE(settings.profile(id).value(QStringLiteral("baseUrl")).toString(),
             QStringLiteral("https://example.invalid"));
    QVERIFY(settings.configured());

    // A legacy caller cannot bypass the rule: the new host is saved only as an
    // unconfigured draft and the old host's credential is cleared.
    QCOMPARE(settings.saveProfile(changed), id);
    QCOMPARE(settings.profile(id).value(QStringLiteral("baseUrl")).toString(),
             QStringLiteral("https://host-b.invalid"));
    QVERIFY(!settings.hasCredential(id));
    QVERIFY(!settings.configured());
    QVERIFY(!settings.testConnection(id));
    QCOMPARE(settings.lastTestStatus(), QStringLiteral("failed"));
    QByteArray plaintext;
    QVERIFY(!settings.credential(id, &plaintext));

    // Supplying a new key binds it to the new endpoint and restores configuration.
    QCOMPARE(settings.saveProfileWithCredential(changed,
                                                 QStringLiteral("host-b-test-key")), id);
    QVERIFY(settings.configured());
    QVERIFY(settings.credential(id, &plaintext));
    QCOMPARE(plaintext, QByteArray("host-b-test-key"));
    CredentialStore::clearSensitive(&plaintext);
}

void ProviderSettingsTest::credentialFailuresLeaveNoPartialProfile()
{
#ifndef Q_OS_WIN
    QSKIP("DPAPI test is Windows-only");
#endif
    QTemporaryDir directory;
    const QString settingsPath = directory.filePath(QStringLiteral("settings.ini"));
    const QString credentialsPath = directory.filePath(QStringLiteral("credentials.ini"));
    ProviderSettings settings(settingsPath, credentialsPath);

    CredentialStore::setProtectFailureForTest(true);
    const QString forcedFailureResult = settings.saveProfileWithCredential(
                validProfile(), QStringLiteral("never-saved-test-key"));
    CredentialStore::setProtectFailureForTest(false);
    QVERIFY(forcedFailureResult.isEmpty());
    QVERIFY(settings.profiles().isEmpty());
    QVERIFY(!settings.configured());
    QVERIFY(!readFile(settingsPath).contains("never-saved-test-key"));
    QVERIFY(!readFile(credentialsPath).contains("never-saved-test-key"));

    // Credential QSettings failure also happens before profile metadata is committed.
    const QString blocker = directory.filePath(QStringLiteral("credential-blocker"));
    QFile blockerFile(blocker);
    QVERIFY(blockerFile.open(QIODevice::WriteOnly));
    blockerFile.write("not-a-directory");
    blockerFile.close();
    ProviderSettings blockedSettings(directory.filePath(QStringLiteral("blocked-settings.ini")),
                                     blocker + QStringLiteral("/credentials.ini"));
    QVERIFY(blockedSettings.saveProfileWithCredential(
                validProfile(), QStringLiteral("blocked-test-key")).isEmpty());
    QVERIFY(blockedSettings.profiles().isEmpty());
    QVERIFY(!blockedSettings.configured());
}

void ProviderSettingsTest::truncatedJsonIsPreserved()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString settingsPath = directory.filePath(QStringLiteral("settings.ini"));
    QSettings raw(settingsPath, QSettings::IniFormat);
    raw.setValue(QStringLiteral("meta/schemaVersion"), 1);
    raw.setValue(QStringLiteral("providers/profilesJson"), QStringLiteral("[{"));
    raw.sync();
    QCOMPARE(raw.status(), QSettings::NoError);
    const QByteArray original = readFile(settingsPath);
    QVERIFY(!original.isEmpty());

    ProviderSettings settings(settingsPath,
                              directory.filePath(QStringLiteral("credentials.ini")));
    QCOMPARE(settings.persistenceStatus(), QStringLiteral("error"));
    QVERIFY(!settings.persistenceMessage().isEmpty());
    QVERIFY(settings.profiles().isEmpty());
    QCOMPARE(readFile(settingsPath), original);
    QCOMPARE(readFile(settings.settingsBackupFilePath()), original);
}

void ProviderSettingsTest::unknownSchemaIsRejected()
{
    QTemporaryDir directory;
    const QString settingsPath = directory.filePath(QStringLiteral("settings.ini"));
    QSettings raw(settingsPath, QSettings::IniFormat);
    raw.setValue(QStringLiteral("meta/schemaVersion"), 999);
    raw.setValue(QStringLiteral("providers/profilesJson"), QStringLiteral("[]"));
    raw.sync();
    const QByteArray original = readFile(settingsPath);

    ProviderSettings settings(settingsPath,
                              directory.filePath(QStringLiteral("credentials.ini")));
    QCOMPARE(settings.persistenceStatus(), QStringLiteral("error"));
    QVERIFY(settings.persistenceMessage().contains(QStringLiteral("schema"),
                                                    Qt::CaseInsensitive));
    QCOMPARE(readFile(settingsPath), original);
    QCOMPARE(readFile(settings.settingsBackupFilePath()), original);
}

void ProviderSettingsTest::legacySettingsMigrateOnExplicitSave()
{
    QTemporaryDir directory;
    const QString settingsPath = directory.filePath(QStringLiteral("settings.ini"));
    QVariantMap legacyProfile = noAuthProfile();
    legacyProfile.insert(QStringLiteral("id"), QStringLiteral("legacy-profile"));
    QSettings raw(settingsPath, QSettings::IniFormat);
    raw.setValue(QStringLiteral("providers/profilesJson"),
                 QString::fromUtf8(QJsonDocument::fromVariant(
                     QVariantList{legacyProfile}).toJson(QJsonDocument::Compact)));
    raw.setValue(QStringLiteral("providers/activeProfileId"),
                 QStringLiteral("legacy-profile"));
    raw.setValue(QStringLiteral("application/startupCompleted"), false);
    raw.sync();
    const QByteArray legacyContents = readFile(settingsPath);

    ProviderSettings settings(settingsPath,
                              directory.filePath(QStringLiteral("credentials.ini")));
    QCOMPARE(settings.persistenceStatus(), QStringLiteral("legacy"));
    QCOMPARE(settings.profiles().size(), 1);
    QCOMPARE(settings.activeProfileId(), QStringLiteral("legacy-profile"));
    QCOMPARE(readFile(settingsPath), legacyContents); // constructor performed no write
    QVERIFY(!QFileInfo::exists(settings.settingsBackupFilePath()));

    settings.setStartupCompleted(true);
    QVERIFY(settings.startupCompleted());
    QCOMPARE(settings.persistenceStatus(), QStringLiteral("ok"));
    QSettings migrated(settingsPath, QSettings::IniFormat);
    QCOMPARE(migrated.value(QStringLiteral("meta/schemaVersion")).toInt(), 1);
    QCOMPARE(readFile(settings.settingsBackupFilePath()), legacyContents);
}

void ProviderSettingsTest::atomicWriteFailureLeavesOriginalUnchanged()
{
    QTemporaryDir directory;
    const QString settingsPath = directory.filePath(QStringLiteral("settings.ini"));
    ProviderSettings settings(settingsPath,
                              directory.filePath(QStringLiteral("credentials.ini")));
    const QString id = settings.saveProfile(noAuthProfile());
    QVERIFY(!id.isEmpty());
    const QByteArray original = readFile(settingsPath);

    QVariantMap changed = settings.profile(id);
    changed.insert(QStringLiteral("name"), QStringLiteral("Must not be committed"));
    ProviderSettings::setPersistFailureForTest(true);
    const QString failedResult = settings.saveProfile(changed);
    ProviderSettings::setPersistFailureForTest(false);
    QVERIFY(failedResult.isEmpty());
    QCOMPARE(readFile(settingsPath), original);
    QCOMPARE(settings.profile(id).value(QStringLiteral("name")).toString(),
             QStringLiteral("Local compatible API"));
    QCOMPARE(readFile(settings.settingsBackupFilePath()), original);
}

void ProviderSettingsTest::validBackupIsRecoveredWithoutOverwritingPrimary()
{
    QTemporaryDir directory;
    const QString settingsPath = directory.filePath(QStringLiteral("settings.ini"));
    const QString credentialPath = directory.filePath(QStringLiteral("credentials.ini"));
    QString id;
    QByteArray expectedBackup;
    {
        ProviderSettings settings(settingsPath, credentialPath);
        id = settings.saveProfile(noAuthProfile());
        QVERIFY(!id.isEmpty());
        QVariantMap changed = settings.profile(id);
        changed.insert(QStringLiteral("name"), QStringLiteral("Second version"));
        QCOMPARE(settings.saveProfile(changed), id);
        expectedBackup = readFile(settings.settingsBackupFilePath());
        QVERIFY(!expectedBackup.isEmpty());
    }

    const QByteArray corruptPrimary("[meta]\nschemaVersion=1\n[providers]\nprofilesJson=[{\n");
    QVERIFY(writeFile(settingsPath, corruptPrimary));
    ProviderSettings recovered(settingsPath, credentialPath);
    QCOMPARE(recovered.persistenceStatus(), QStringLiteral("recovered"));
    QCOMPARE(recovered.profiles().size(), 1);
    QCOMPARE(recovered.profile(id).value(QStringLiteral("name")).toString(),
             QStringLiteral("Local compatible API"));
    QCOMPARE(readFile(settingsPath), corruptPrimary);
    QCOMPARE(readFile(recovered.settingsBackupFilePath()), expectedBackup);
}

void ProviderSettingsTest::startupAndTestConnectionContract()
{
    QTemporaryDir directory;
    const QString settingsPath = directory.filePath(QStringLiteral("settings.ini"));
    const QString credentialsPath = directory.filePath(QStringLiteral("credentials.ini"));
    ProviderSettings settings(settingsPath, credentialsPath);
    QVERIFY(!settings.startupCompleted());
    settings.setStartupCompleted(true);
    QVERIFY(settings.startupCompleted());

    const QString id = settings.saveProfile(noAuthProfile());
    QSignalSpy spy(&settings, SIGNAL(testConnectionRequested(QString)));
    QVERIFY(settings.testConnection(id));
    QCOMPARE(settings.lastTestStatus(), QStringLiteral("testing"));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toString(), id);
    settings.setTestResult(QStringLiteral("succeeded"), QStringLiteral("HTTP 200"));
    QCOMPARE(settings.lastTestStatus(), QStringLiteral("succeeded"));
    QCOMPARE(settings.lastTestMessage(), QStringLiteral("HTTP 200"));

    ProviderSettings reloaded(settingsPath, credentialsPath);
    QVERIFY(reloaded.startupCompleted());
}

void ProviderSettingsTest::modelSelectionAndReasoningContract()
{
    QTemporaryDir directory;
    ProviderSettings settings(directory.filePath(QStringLiteral("settings.ini")),
                              directory.filePath(QStringLiteral("credentials.ini")));
    const QString id = settings.saveProfile(validProfile());
    QVERIFY(settings.setCredential(id, QStringLiteral("temporary-test-key")));

    QSignalSpy discoverySpy(&settings, &ProviderSettings::modelDiscoveryRequested);
    QVERIFY(settings.refreshModels());
    QCOMPARE(discoverySpy.count(), 1);
    settings.setDiscoveredModels(id, {QStringLiteral("deepseek-v4-flash"),
                                      QStringLiteral("deepseek-v4-pro")});
    QCOMPARE(settings.availableModels().size(), 2);
    QVERIFY(settings.setActiveModel(QStringLiteral("deepseek-v4-pro")));
    QCOMPARE(settings.activeModel(), QStringLiteral("deepseek-v4-pro"));

    QVERIFY(settings.setActiveReasoningMode(QStringLiteral("off")));
    QCOMPARE(settings.activeReasoningMode(), QStringLiteral("off"));
    QVERIFY(!settings.profile(id).value(QStringLiteral("thinkingEnabled")).toBool());
    QVERIFY(settings.setActiveReasoningMode(QStringLiteral("xhigh")));
    QCOMPARE(settings.activeReasoningMode(), QStringLiteral("xhigh"));
    QVERIFY(settings.profile(id).value(QStringLiteral("thinkingEnabled")).toBool());
    QVERIFY(settings.setActiveReasoningMode(QStringLiteral("max")));
    QCOMPARE(settings.profile(id).value(QStringLiteral("reasoningEffort")).toString(),
             QStringLiteral("max"));
}

void ProviderSettingsTest::loggerRedactsAndRotates()
{
    QTemporaryDir directory;
    const QString path = directory.filePath(QStringLiteral("logs/app.jsonl"));
    JsonlLogger logger(path, 1024, 2);
    const QString canary = QStringLiteral("secret-canary-12345");
    logger.addSensitiveValue(canary);
    QVariantMap fields = {
        {QStringLiteral("authorization"), QStringLiteral("Bearer should-not-appear")},
        {QStringLiteral("message"), QStringLiteral("failed with ") + canary},
        {QStringLiteral("url"), QStringLiteral("https://host/v1?api_key=query-secret")},
        {QStringLiteral("nested"), QVariantMap{{QStringLiteral("content"), QStringLiteral("private prompt")}}},
        {QStringLiteral("detail"), QString(850, QLatin1Char('x'))}
    };
    QVERIFY2(logger.write(QStringLiteral("provider"), QStringLiteral("request_failed"), fields,
                          QStringLiteral("warning")), qPrintable(logger.errorString()));
    QVERIFY(logger.write(QStringLiteral("provider"), QStringLiteral("request_failed"), fields));
    QVERIFY(QFile::exists(path + QStringLiteral(".1")));

    const QByteArray all = readFile(path) + readFile(path + QStringLiteral(".1"));
    QVERIFY(!all.contains(canary.toUtf8()));
    QVERIFY(!all.contains("should-not-appear"));
    QVERIFY(!all.contains("query-secret"));
    QVERIFY(!all.contains("private prompt"));
    QVERIFY(all.contains("[REDACTED]"));

    const QList<QByteArray> lines = readFile(path).split('\n');
    QVERIFY(!lines.first().isEmpty());
    const QJsonDocument document = QJsonDocument::fromJson(lines.first());
    QVERIFY(document.isObject());
    QVERIFY(document.object().contains(QStringLiteral("timestamp")));
}

QTEST_MAIN(ProviderSettingsTest)
#include "test_provider_settings.moc"
