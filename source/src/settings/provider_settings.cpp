#include "provider_settings.h"
#include "privacy/privacy_consent_service.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>
#include <QUuid>

namespace smartkey {
namespace {

const char kProfilesKey[] = "providers/profilesJson";
const char kActiveProfileKey[] = "providers/activeProfileId";
const char kStartupCompletedKey[] = "application/startupCompleted";
const char kSchemaVersionKey[] = "meta/schemaVersion";
const int kCurrentSchemaVersion = 1;
#ifdef SMARTKEY_SETTINGS_TESTS
bool g_forcePersistFailure = false;
#endif

QString newProfileId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

bool persistedIdIsSafe(const QString &id)
{
    static const QRegularExpression allowed(QStringLiteral("^[A-Za-z0-9_-]{1,64}$"));
    return allowed.match(id).hasMatch();
}

QVariantMap removeSecretLikeValues(QVariantMap values)
{
    // These are ignored even if a caller attempts to smuggle them into a profile map.
    static const QStringList forbidden = {
        QStringLiteral("apiKey"), QStringLiteral("api_key"), QStringLiteral("credential"),
        QStringLiteral("password"), QStringLiteral("secret"), QStringLiteral("token"),
        QStringLiteral("authorization"), QStringLiteral("authValue")
    };
    for (const QString &key : forbidden)
        values.remove(key);
    return values;
}

} // namespace

ProviderSettings::ProviderSettings(QObject *parent)
    : ProviderSettings(defaultSettingsFilePath(), CredentialStore::defaultStorageFilePath(), parent)
{
}

ProviderSettings::ProviderSettings(const QString &settingsFilePath,
                                   const QString &credentialFilePath, QObject *parent)
    : QObject(parent),
      m_settingsFilePath(settingsFilePath),
      m_credentials(credentialFilePath)
{
    load();
}

QString ProviderSettings::defaultSettingsFilePath()
{
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    return QDir(directory).filePath(QStringLiteral("settings.ini"));
}

void ProviderSettings::setPrivacyConsentService(PrivacyConsentService *service)
{
    m_privacyConsent = service;
}

QVariantList ProviderSettings::profiles() const
{
    QVariantList result;
    result.reserve(m_profiles.size());
    for (const ProviderProfile &item : m_profiles)
        result.append(item.toVariantMap(hasBoundCredential(item)));
    return result;
}

bool ProviderSettings::configured() const
{
    const int index = indexOf(m_activeProfileId);
    if (index < 0)
        return false;
    const ProviderProfile &item = m_profiles.at(index);
    return item.isValid() && (!item.requiresCredential() || hasBoundCredential(item));
}

QString ProviderSettings::activeModel() const
{
    const int index = indexOf(m_activeProfileId);
    return index < 0 ? QString() : m_profiles.at(index).model;
}

QString ProviderSettings::activeReasoningMode() const
{
    const int index = indexOf(m_activeProfileId);
    if (index < 0 || !m_profiles.at(index).thinkingEnabled)
        return QStringLiteral("off");
    return m_profiles.at(index).reasoningEffort;
}

QString ProviderSettings::saveProfile(const QVariantMap &input)
{
    // This compatibility path may create an unconfigured draft.  If an existing
    // endpoint/authentication binding changes, saveProfileInternal clears the old
    // credential before committing so it can never be reused for the new host.
    return saveProfileInternal(input, QString(), false);
}

QString ProviderSettings::saveProfileWithCredential(const QVariantMap &input,
                                                    const QString &plaintext)
{
    return saveProfileInternal(input, plaintext, true);
}

QString ProviderSettings::saveProfileInternal(const QVariantMap &input,
                                              const QString &plaintext,
                                              bool requireCompleteCredential)
{
    const QString operation = requireCompleteCredential
            ? QStringLiteral("saveProfileWithCredential") : QStringLiteral("saveProfile");
    const bool wasConfigured = configured();
    const QString oldActive = m_activeProfileId;
    const QVariantMap values = removeSecretLikeValues(input);
    const QString requestedId = values.value(QStringLiteral("id")).toString();
    int index = indexOf(requestedId);
    const QString finalId = index >= 0 ? requestedId : newProfileId();
    const ProviderProfile newValue = ProviderProfile::fromVariantMap(values, finalId);
    QString validationError;
    if (!newValue.isValid(&validationError)) {
        emit operationFailed(operation, validationError);
        return {};
    }

    const bool existing = index >= 0;
    const QByteArray newBinding = credentialBindingFingerprint(newValue);
    const QByteArray oldBinding = existing
            ? credentialBindingFingerprint(m_profiles.at(index)) : QByteArray();
    const bool bindingChanged = existing && oldBinding != newBinding;
    const bool suppliedCredential = !plaintext.isEmpty();
    if (requireCompleteCredential && newValue.requiresCredential()
        && !suppliedCredential
        && (!existing || bindingChanged || !hasBoundCredential(m_profiles.at(index)))) {
        emit operationFailed(operation,
                             QStringLiteral("A new credential is required when the provider "
                                            "endpoint or authentication changes"));
        return {};
    }

    const CredentialStore::Snapshot oldCredential = m_credentials.snapshot(finalId);
    bool credentialChanged = false;
    QString credentialError;
    if (!newValue.requiresCredential()) {
        if (!m_credentials.clearCredential(finalId, &credentialError)) {
            emit operationFailed(operation, credentialError);
            return {};
        }
        credentialChanged = oldCredential.existed;
    } else if (suppliedCredential) {
        if (!m_credentials.setCredential(finalId, plaintext, newBinding, &credentialError)) {
            emit operationFailed(operation, credentialError);
            return {};
        }
        credentialChanged = true;
    } else if (bindingChanged) {
        if (!m_credentials.clearCredential(finalId, &credentialError)) {
            emit operationFailed(operation, credentialError);
            return {};
        }
        credentialChanged = oldCredential.existed;
    }

    const QList<ProviderProfile> oldProfiles = m_profiles;
    if (index >= 0)
        m_profiles[index] = newValue;
    else
        m_profiles.append(newValue);
    if (m_activeProfileId.isEmpty())
        m_activeProfileId = finalId;

    if (!persist()) {
        m_profiles = oldProfiles;
        m_activeProfileId = oldActive;
        QString rollbackError;
        const bool rolledBack = !credentialChanged
                || m_credentials.restore(finalId, oldCredential, &rollbackError);
        emit operationFailed(operation,
                             rolledBack
                             ? QStringLiteral("Failed to persist provider settings; changes were rolled back")
                             : QStringLiteral("Failed to persist provider settings and restore credential: %1")
                                   .arg(rollbackError));
        return {};
    }
    if (finalId == m_activeProfileId)
        resetModelDiscovery();
    emit profilesChanged();
    emit activeSelectionChanged();
    emitConfigurationChanges(wasConfigured, oldActive);
    return finalId;
}

bool ProviderSettings::removeProfile(const QString &id)
{
    const int index = indexOf(id);
    if (index < 0)
        return false;
    const bool wasConfigured = configured();
    const QString oldActive = m_activeProfileId;
    const QList<ProviderProfile> oldProfiles = m_profiles;
    const CredentialStore::Snapshot oldCredential = m_credentials.snapshot(id);

    QString credentialError;
    if (!m_credentials.clearCredential(id, &credentialError)) {
        emit operationFailed(QStringLiteral("removeProfile"), credentialError);
        return false;
    }

    m_profiles.removeAt(index);
    if (m_activeProfileId == id)
        m_activeProfileId = m_profiles.isEmpty() ? QString() : m_profiles.first().id;
    if (!persist()) {
        m_profiles = oldProfiles;
        m_activeProfileId = oldActive;
        QString rollbackError;
        m_credentials.restore(id, oldCredential, &rollbackError);
        emit operationFailed(QStringLiteral("removeProfile"),
                             rollbackError.isEmpty()
                             ? QStringLiteral("Failed to persist provider settings; changes were rolled back")
                             : QStringLiteral("Failed to persist provider settings and restore credential: %1")
                                   .arg(rollbackError));
        return false;
    }

    emit profilesChanged();
    emit activeSelectionChanged();
    resetModelDiscovery();
    emitConfigurationChanges(wasConfigured, oldActive);
    return true;
}

bool ProviderSettings::setActiveProfile(const QString &id)
{
    if (indexOf(id) < 0 || m_activeProfileId == id)
        return indexOf(id) >= 0;
    const bool wasConfigured = configured();
    const QString oldActive = m_activeProfileId;
    m_activeProfileId = id;
    if (!persist()) {
        m_activeProfileId = oldActive;
        emit operationFailed(QStringLiteral("setActiveProfile"),
                             QStringLiteral("Failed to persist the active profile"));
        return false;
    }
    emitConfigurationChanges(wasConfigured, oldActive);
    emit activeSelectionChanged();
    resetModelDiscovery();
    return true;
}

bool ProviderSettings::setCredential(const QString &id, const QString &plaintext)
{
    const int index = indexOf(id);
    if (index < 0) {
        emit operationFailed(QStringLiteral("setCredential"), QStringLiteral("Unknown profile ID"));
        return false;
    }
    const bool wasConfigured = configured();
    const CredentialStore::Snapshot oldCredential = m_credentials.snapshot(id);
    QString error;
    if (!m_credentials.setCredential(id, plaintext,
                                     credentialBindingFingerprint(m_profiles.at(index)), &error)) {
        emit operationFailed(QStringLiteral("setCredential"), error);
        return false;
    }
    if (!persist()) {
        QString rollbackError;
        m_credentials.restore(id, oldCredential, &rollbackError);
        emit operationFailed(QStringLiteral("setCredential"),
                             rollbackError.isEmpty()
                             ? QStringLiteral("Failed to persist credential state; changes were rolled back")
                             : QStringLiteral("Failed to persist credential state and restore credential: %1")
                                   .arg(rollbackError));
        return false;
    }
    emit profilesChanged();
    if (wasConfigured != configured())
        emit configuredChanged();
    return true;
}

bool ProviderSettings::clearCredential(const QString &id)
{
    if (indexOf(id) < 0)
        return false;
    const bool wasConfigured = configured();
    const CredentialStore::Snapshot oldCredential = m_credentials.snapshot(id);
    QString error;
    if (!m_credentials.clearCredential(id, &error)) {
        emit operationFailed(QStringLiteral("clearCredential"), error);
        return false;
    }
    if (!persist()) {
        QString rollbackError;
        m_credentials.restore(id, oldCredential, &rollbackError);
        emit operationFailed(QStringLiteral("clearCredential"),
                             rollbackError.isEmpty()
                             ? QStringLiteral("Failed to persist credential state; changes were rolled back")
                             : QStringLiteral("Failed to persist credential state and restore credential: %1")
                                   .arg(rollbackError));
        return false;
    }
    emit profilesChanged();
    if (wasConfigured != configured())
        emit configuredChanged();
    return true;
}

bool ProviderSettings::clearAllCredentials(QString *errorMessage)
{
    struct SavedCredential {
        QString id;
        CredentialStore::Snapshot snapshot;
    };
    QList<SavedCredential> saved;
    saved.reserve(m_profiles.size());
    for (const ProviderProfile &profile : m_profiles)
        saved.append({profile.id, m_credentials.snapshot(profile.id)});

    const bool wasConfigured = configured();
    QString error;
    for (const SavedCredential &credential : saved) {
        if (!m_credentials.clearCredential(credential.id, &error)) {
            for (const SavedCredential &restoreValue : saved)
                m_credentials.restore(restoreValue.id, restoreValue.snapshot, nullptr);
            if (errorMessage)
                *errorMessage = error;
            emit operationFailed(QStringLiteral("clearAllCredentials"), error);
            return false;
        }
    }
    if (!persist()) {
        QString rollbackError;
        for (const SavedCredential &restoreValue : saved) {
            QString currentError;
            if (!m_credentials.restore(restoreValue.id, restoreValue.snapshot, &currentError)
                    && rollbackError.isEmpty())
                rollbackError = currentError;
        }
        const QString message = rollbackError.isEmpty()
                ? QStringLiteral("Failed to persist credential removal; changes were rolled back")
                : QStringLiteral("Failed to persist credential removal and rollback: %1")
                      .arg(rollbackError);
        if (errorMessage)
            *errorMessage = message;
        emit operationFailed(QStringLiteral("clearAllCredentials"), message);
        return false;
    }
    if (errorMessage)
        errorMessage->clear();
    emit profilesChanged();
    if (wasConfigured != configured())
        emit configuredChanged();
    return true;
}

bool ProviderSettings::hasCredential(const QString &id) const
{
    const int index = indexOf(id);
    return index >= 0 && hasBoundCredential(m_profiles.at(index));
}

QVariantMap ProviderSettings::profile(const QString &id) const
{
    const int index = indexOf(id);
    return index < 0 ? QVariantMap()
                     : m_profiles.at(index).toVariantMap(hasBoundCredential(m_profiles.at(index)));
}

bool ProviderSettings::testConnection(const QString &id)
{
    if (!networkConsentGranted(QStringLiteral("provider_test"))) {
        setTestResult(QStringLiteral("failed"),
                      tr("请先阅读并同意当前隐私说明。"));
        return false;
    }
    const int index = indexOf(id);
    if (index < 0) {
        setTestResult(QStringLiteral("failed"), QStringLiteral("Unknown provider profile"));
        return false;
    }
    const ProviderProfile &profile = m_profiles.at(index);
    if (!profile.isValid() || (profile.requiresCredential() && !hasBoundCredential(profile))) {
        setTestResult(QStringLiteral("failed"),
                      QStringLiteral("Provider credential is missing or belongs to a different endpoint"));
        return false;
    }
    setTestResult(QStringLiteral("testing"), QString());
    emit testConnectionRequested(id);
    return true;
}

bool ProviderSettings::refreshModels()
{
    if (!networkConsentGranted(QStringLiteral("model_discovery"))) {
        m_modelDiscoveryInProgress = false;
        m_modelDiscoveryMessage = tr("请先阅读并同意当前隐私说明。未发起模型检测请求。");
        emit modelDiscoveryChanged();
        return false;
    }
    const int index = indexOf(m_activeProfileId);
    if (index < 0 || m_modelDiscoveryInProgress)
        return false;
    const ProviderProfile &profile = m_profiles.at(index);
    const bool deepSeek = profile.providerType == QLatin1String("deepseek")
            || QUrl(profile.baseUrl).host().compare(QStringLiteral("api.deepseek.com"),
                                                    Qt::CaseInsensitive) == 0;
    if (!deepSeek) {
        m_modelDiscoveryMessage = tr("当前仅支持自动检测 DeepSeek 模型。");
        emit modelDiscoveryChanged();
        return false;
    }
    if (profile.requiresCredential() && !hasBoundCredential(profile)) {
        m_modelDiscoveryMessage = tr("请先在设置中保存 DeepSeek API Key。");
        emit modelDiscoveryChanged();
        return false;
    }
    m_modelDiscoveryInProgress = true;
    m_modelDiscoveryMessage = tr("正在检测可用模型…");
    emit modelDiscoveryChanged();
    emit modelDiscoveryRequested(profile.id);
    return true;
}

bool ProviderSettings::setActiveModel(const QString &model)
{
    const int index = indexOf(m_activeProfileId);
    const QString value = model.trimmed();
    if (index < 0 || value.isEmpty())
        return false;
    if (!m_availableModels.isEmpty() && !m_availableModels.contains(value)) {
        emit operationFailed(QStringLiteral("setActiveModel"),
                             QStringLiteral("The selected model is not in the discovered list"));
        return false;
    }
    if (m_profiles[index].model == value)
        return true;
    const QString previous = m_profiles[index].model;
    m_profiles[index].model = value;
    if (!persist()) {
        m_profiles[index].model = previous;
        emit operationFailed(QStringLiteral("setActiveModel"),
                             QStringLiteral("Failed to persist the selected model"));
        return false;
    }
    emit profilesChanged();
    emit activeSelectionChanged();
    return true;
}

bool ProviderSettings::setActiveReasoningMode(const QString &mode)
{
    const int index = indexOf(m_activeProfileId);
    if (index < 0)
        return false;
    ProviderProfile &profile = m_profiles[index];
    const bool deepSeek = profile.providerType == QLatin1String("deepseek")
            || QUrl(profile.baseUrl).host().compare(QStringLiteral("api.deepseek.com"),
                                                    Qt::CaseInsensitive) == 0;
    if (!deepSeek)
        return false;

    QString value = mode.trimmed().toLower();
    if (value == QLatin1String("medium"))
        value = QStringLiteral("high");
    const QStringList allowed = {QStringLiteral("off"), QStringLiteral("low"),
                                 QStringLiteral("high"), QStringLiteral("xhigh"),
                                 QStringLiteral("max")};
    if (!allowed.contains(value))
        return false;
    const bool enabled = value != QLatin1String("off");
    const QString effort = enabled ? value : profile.reasoningEffort;
    if (profile.thinkingEnabled == enabled && profile.reasoningEffort == effort)
        return true;

    const bool previousEnabled = profile.thinkingEnabled;
    const QString previousEffort = profile.reasoningEffort;
    profile.thinkingEnabled = enabled;
    profile.supportsReasoning = true;
    profile.reasoningEffort = effort;
    if (!persist()) {
        profile.thinkingEnabled = previousEnabled;
        profile.reasoningEffort = previousEffort;
        emit operationFailed(QStringLiteral("setActiveReasoningMode"),
                             QStringLiteral("Failed to persist the reasoning mode"));
        return false;
    }
    emit profilesChanged();
    emit activeSelectionChanged();
    return true;
}

void ProviderSettings::setDiscoveredModels(const QString &profileId,
                                           const QStringList &models)
{
    if (profileId != m_activeProfileId)
        return;
    QStringList clean;
    for (const QString &model : models) {
        const QString value = model.trimmed();
        if (!value.isEmpty() && !clean.contains(value))
            clean.append(value);
    }
    m_availableModels = clean;
    m_modelDiscoveryInProgress = false;
    m_modelDiscoveryMessage = clean.isEmpty()
            ? tr("没有检测到可用模型。")
            : tr("已检测到 %1 个可用模型").arg(clean.size());
    emit modelDiscoveryChanged();
}

void ProviderSettings::setModelDiscoveryError(const QString &profileId,
                                              const QString &message)
{
    if (profileId != m_activeProfileId)
        return;
    m_modelDiscoveryInProgress = false;
    m_modelDiscoveryMessage = message.isEmpty() ? tr("模型检测失败。") : message;
    emit modelDiscoveryChanged();
}

bool ProviderSettings::credential(const QString &id, QByteArray *plaintext,
                                  QString *errorMessage) const
{
    const int index = indexOf(id);
    if (index < 0) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Unknown profile ID");
        return false;
    }
    return m_credentials.credential(id,
                                    credentialBindingFingerprint(m_profiles.at(index)),
                                    plaintext, errorMessage);
}

void ProviderSettings::setTestResult(const QString &status, const QString &message)
{
    const QString normalized = status.trimmed().toLower();
    static const QStringList allowed = {
        QStringLiteral("idle"), QStringLiteral("testing"), QStringLiteral("succeeded"),
        QStringLiteral("failed")
    };
    const QString finalStatus = allowed.contains(normalized) ? normalized : QStringLiteral("failed");
    if (m_lastTestStatus == finalStatus && m_lastTestMessage == message)
        return;
    m_lastTestStatus = finalStatus;
    m_lastTestMessage = message;
    emit testResultChanged();
}

void ProviderSettings::setStartupCompleted(bool completed)
{
    if (m_startupCompleted == completed)
        return;
    m_startupCompleted = completed;
    if (!persist()) {
        m_startupCompleted = !completed;
        emit operationFailed(QStringLiteral("setStartupCompleted"),
                             QStringLiteral("Failed to persist startup state"));
        return;
    }
    emit startupCompletedChanged();
}

int ProviderSettings::indexOf(const QString &id) const
{
    for (int i = 0; i < m_profiles.size(); ++i) {
        if (m_profiles.at(i).id == id)
            return i;
    }
    return -1;
}

QByteArray ProviderSettings::credentialBindingFingerprint(const ProviderProfile &profile) const
{
    const QUrl url(profile.baseUrl);
    const QByteArray binding = url.toEncoded(QUrl::FullyEncoded) + '\n'
            + profile.chatPath.toUtf8() + '\n'
            + profile.apiStyle.trimmed().toLower().toUtf8() + '\n'
            + profile.authScheme.trimmed().toLower().toUtf8() + '\n'
            + profile.authHeaderName.trimmed().toLower().toUtf8();
    return QCryptographicHash::hash(binding, QCryptographicHash::Sha256);
}

bool ProviderSettings::hasBoundCredential(const ProviderProfile &profile) const
{
    return m_credentials.hasCredential(profile.id, credentialBindingFingerprint(profile));
}

bool ProviderSettings::networkConsentGranted(const QString &action)
{
    return !m_privacyConsent || m_privacyConsent->requireConsent(action);
}

bool ProviderSettings::parseSettingsFile(const QString &path, LoadedState *state,
                                         QString *errorMessage) const
{
    if (!state) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Internal settings parser error");
        return false;
    }

    QSettings settings(path, QSettings::IniFormat);
    if (settings.status() != QSettings::NoError) {
        if (errorMessage)
            *errorMessage = QStringLiteral("The settings INI file cannot be parsed");
        return false;
    }

    LoadedState parsed;
    const QVariant schemaValue = settings.value(QLatin1String(kSchemaVersionKey));
    if (schemaValue.isValid()) {
        bool ok = false;
        const int schemaVersion = schemaValue.toInt(&ok);
        if (!ok || schemaVersion != kCurrentSchemaVersion) {
            if (errorMessage)
                *errorMessage = QStringLiteral("Unsupported settings schema version: %1")
                        .arg(schemaValue.toString());
            return false;
        }
    } else {
        parsed.legacySchema = true;
    }

    if (settings.contains(QLatin1String(kProfilesKey))) {
        const QByteArray json = settings.value(QLatin1String(kProfilesKey)).toByteArray();
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isArray()) {
            if (errorMessage)
                *errorMessage = QStringLiteral("Provider profile JSON is invalid: %1")
                        .arg(parseError.errorString());
            return false;
        }

        const QVariantList storedProfiles = document.toVariant().toList();
        for (const QVariant &value : storedProfiles) {
            if (value.type() != QVariant::Map) {
                if (errorMessage)
                    *errorMessage = QStringLiteral("Provider profile entry is not an object");
                return false;
            }
            const QVariantMap map = removeSecretLikeValues(value.toMap());
            const QString id = map.value(QStringLiteral("id")).toString();
            bool duplicate = false;
            for (const ProviderProfile &existing : parsed.profiles) {
                if (existing.id == id) {
                    duplicate = true;
                    break;
                }
            }
            if (!persistedIdIsSafe(id) || duplicate) {
                if (errorMessage)
                    *errorMessage = QStringLiteral("Provider profile has an invalid or duplicate ID");
                return false;
            }
            const ProviderProfile item = ProviderProfile::fromVariantMap(map, id);
            QString validationError;
            if (!item.isValid(&validationError)) {
                if (errorMessage)
                    *errorMessage = QStringLiteral("Provider profile is invalid: %1")
                            .arg(validationError);
                return false;
            }
            parsed.profiles.append(item);
        }
    }

    parsed.activeProfileId = settings.value(QLatin1String(kActiveProfileKey)).toString();
    bool activeFound = parsed.activeProfileId.isEmpty();
    for (const ProviderProfile &item : parsed.profiles) {
        if (item.id == parsed.activeProfileId) {
            activeFound = true;
            break;
        }
    }
    if (!activeFound || parsed.activeProfileId.isEmpty())
        parsed.activeProfileId = parsed.profiles.isEmpty() ? QString() : parsed.profiles.first().id;
    parsed.startupCompleted = settings.value(QLatin1String(kStartupCompletedKey), false).toBool();
    *state = parsed;
    return true;
}

QByteArray ProviderSettings::serializedSettings(QString *errorMessage) const
{
    const QString directory = QFileInfo(m_settingsFilePath).absolutePath();
    if (!QDir().mkpath(directory)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Failed to create the settings directory");
        return {};
    }

    const QString temporaryPath = QDir(directory).filePath(
                QStringLiteral(".provider-settings-%1.ini")
                    .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));

    QVariantList profiles;
    profiles.reserve(m_profiles.size());
    for (const ProviderProfile &item : m_profiles)
        profiles.append(item.toVariantMap(hasBoundCredential(item)));

    QSettings settings(temporaryPath, QSettings::IniFormat);
    settings.clear();
    settings.setValue(QLatin1String(kSchemaVersionKey), kCurrentSchemaVersion);
    settings.setValue(QLatin1String(kProfilesKey),
                      QString::fromUtf8(QJsonDocument::fromVariant(profiles)
                                            .toJson(QJsonDocument::Compact)));
    settings.setValue(QLatin1String(kActiveProfileKey), m_activeProfileId);
    settings.setValue(QLatin1String(kStartupCompletedKey), m_startupCompleted);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Failed to serialize provider settings (status %1)")
                    .arg(static_cast<int>(settings.status()));
        QFile::remove(temporaryPath);
        return {};
    }

    QFile file(temporaryPath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Failed to read serialized provider settings");
        QFile::remove(temporaryPath);
        return {};
    }
    const QByteArray contents = file.readAll();
    file.close();
    QFile::remove(temporaryPath);
    return contents;
}

bool ProviderSettings::writeFileAtomically(const QString &path, const QByteArray &contents,
                                           QString *errorMessage) const
{
    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)
        || file.write(contents) != contents.size()
        || !file.commit()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Failed to atomically write %1").arg(path);
        file.cancelWriting();
        return false;
    }
    return true;
}

void ProviderSettings::applyLoadedState(const LoadedState &state)
{
    m_profiles = state.profiles;
    m_activeProfileId = state.activeProfileId;
    m_startupCompleted = state.startupCompleted;
}

void ProviderSettings::setPersistenceStatus(const QString &status, const QString &message)
{
    if (m_persistenceStatus == status && m_persistenceMessage == message)
        return;
    m_persistenceStatus = status;
    m_persistenceMessage = message;
    emit persistenceStatusChanged();
}

bool ProviderSettings::persist()
{
    QString error;
    const QByteArray nextContents = serializedSettings(&error);
    if (nextContents.isEmpty()) {
        setPersistenceStatus(QStringLiteral("error"), error);
        return false;
    }

    const QFileInfo currentInfo(m_settingsFilePath);
    if (currentInfo.exists()) {
        if (!currentInfo.isFile()) {
            setPersistenceStatus(QStringLiteral("error"),
                                 QStringLiteral("The settings path is not a regular file"));
            return false;
        }
        LoadedState currentState;
        if (!parseSettingsFile(m_settingsFilePath, &currentState, &error)) {
            setPersistenceStatus(QStringLiteral("error"),
                                 QStringLiteral("Refusing to overwrite invalid settings: %1")
                                     .arg(error));
            return false;
        }
        QFile currentFile(m_settingsFilePath);
        if (!currentFile.open(QIODevice::ReadOnly)) {
            setPersistenceStatus(QStringLiteral("error"),
                                 QStringLiteral("Failed to read the current settings file"));
            return false;
        }
        const QByteArray currentContents = currentFile.readAll();
        if (!writeFileAtomically(settingsBackupFilePath(), currentContents, &error)) {
            setPersistenceStatus(QStringLiteral("error"), error);
            return false;
        }
    }

#ifdef SMARTKEY_SETTINGS_TESTS
    if (g_forcePersistFailure) {
        setPersistenceStatus(QStringLiteral("error"),
                             QStringLiteral("Forced atomic settings write failure"));
        return false;
    }
#endif

    if (!writeFileAtomically(m_settingsFilePath, nextContents, &error)) {
        setPersistenceStatus(QStringLiteral("error"), error);
        return false;
    }
    setPersistenceStatus(QStringLiteral("ok"), QString());
    return true;
}

void ProviderSettings::load()
{
    const QFileInfo primaryInfo(m_settingsFilePath);
    if (!primaryInfo.exists()) {
        resetModelDiscovery();
        setPersistenceStatus(QStringLiteral("ok"), QString());
        return;
    }

    LoadedState state;
    QString primaryError;
    if (primaryInfo.isFile()
        && parseSettingsFile(m_settingsFilePath, &state, &primaryError)) {
        applyLoadedState(state);
        resetModelDiscovery();
        setPersistenceStatus(state.legacySchema ? QStringLiteral("legacy")
                                                : QStringLiteral("ok"),
                             state.legacySchema
                             ? QStringLiteral("Legacy settings loaded; they will be migrated on the next save")
                             : QString());
        return;
    }
    if (primaryError.isEmpty())
        primaryError = QStringLiteral("The settings path is not a regular file");

    LoadedState backupState;
    QString backupError;
    if (QFileInfo::exists(settingsBackupFilePath())
        && parseSettingsFile(settingsBackupFilePath(), &backupState, &backupError)) {
        applyLoadedState(backupState);
        resetModelDiscovery();
        setPersistenceStatus(QStringLiteral("recovered"),
                             QStringLiteral("Primary settings were left untouched; using the valid backup in memory: %1")
                                 .arg(primaryError));
        return;
    }

    // If no last-known-good backup exists, preserve an exact diagnostic copy of
    // the invalid primary without changing the primary itself.
    if (!QFileInfo::exists(settingsBackupFilePath()) && primaryInfo.isFile()) {
        QFile primary(m_settingsFilePath);
        if (primary.open(QIODevice::ReadOnly)) {
            QString ignored;
            writeFileAtomically(settingsBackupFilePath(), primary.readAll(), &ignored);
        }
    }
    resetModelDiscovery();
    setPersistenceStatus(QStringLiteral("error"),
                         QStringLiteral("Provider settings were not loaded and the original file was preserved: %1")
                             .arg(primaryError));
}

#ifdef SMARTKEY_SETTINGS_TESTS
void ProviderSettings::setPersistFailureForTest(bool enabled)
{
    g_forcePersistFailure = enabled;
}
#endif

void ProviderSettings::emitConfigurationChanges(bool wasConfigured,
                                                const QString &oldActiveProfileId)
{
    if (oldActiveProfileId != m_activeProfileId)
        emit activeProfileIdChanged();
    if (wasConfigured != configured())
        emit configuredChanged();
}

void ProviderSettings::resetModelDiscovery()
{
    m_availableModels.clear();
    const int index = indexOf(m_activeProfileId);
    if (index >= 0 && !m_profiles.at(index).model.isEmpty())
        m_availableModels.append(m_profiles.at(index).model);
    m_modelDiscoveryInProgress = false;
    m_modelDiscoveryMessage.clear();
    emit modelDiscoveryChanged();
}

} // namespace smartkey
