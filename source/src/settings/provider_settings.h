#pragma once

#include "credential_store.h"
#include "provider_profile.h"

#include <QObject>
#include <QStringList>
#include <QVariantList>

namespace smartkey {

class PrivacyConsentService;

class ProviderSettings final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList profiles READ profiles NOTIFY profilesChanged)
    Q_PROPERTY(QString activeProfileId READ activeProfileId NOTIFY activeProfileIdChanged)
    Q_PROPERTY(bool configured READ configured NOTIFY configuredChanged)
    Q_PROPERTY(QString lastTestStatus READ lastTestStatus NOTIFY testResultChanged)
    Q_PROPERTY(QString lastTestMessage READ lastTestMessage NOTIFY testResultChanged)
    Q_PROPERTY(QString activeModel READ activeModel NOTIFY activeSelectionChanged)
    Q_PROPERTY(QString activeReasoningMode READ activeReasoningMode NOTIFY activeSelectionChanged)
    Q_PROPERTY(QStringList availableModels READ availableModels NOTIFY modelDiscoveryChanged)
    Q_PROPERTY(bool modelDiscoveryInProgress READ modelDiscoveryInProgress
               NOTIFY modelDiscoveryChanged)
    Q_PROPERTY(QString modelDiscoveryMessage READ modelDiscoveryMessage
               NOTIFY modelDiscoveryChanged)
    Q_PROPERTY(bool startupCompleted READ startupCompleted WRITE setStartupCompleted
               NOTIFY startupCompletedChanged)
    Q_PROPERTY(QString persistenceStatus READ persistenceStatus NOTIFY persistenceStatusChanged)
    Q_PROPERTY(QString persistenceMessage READ persistenceMessage NOTIFY persistenceStatusChanged)

public:
    explicit ProviderSettings(QObject *parent = nullptr);
    ProviderSettings(const QString &settingsFilePath, const QString &credentialFilePath,
                     QObject *parent = nullptr);

    QVariantList profiles() const;
    QString activeProfileId() const { return m_activeProfileId; }
    bool configured() const;
    QString lastTestStatus() const { return m_lastTestStatus; }
    QString lastTestMessage() const { return m_lastTestMessage; }
    QString activeModel() const;
    QString activeReasoningMode() const;
    QStringList availableModels() const { return m_availableModels; }
    bool modelDiscoveryInProgress() const { return m_modelDiscoveryInProgress; }
    QString modelDiscoveryMessage() const { return m_modelDiscoveryMessage; }
    bool startupCompleted() const { return m_startupCompleted; }
    QString persistenceStatus() const { return m_persistenceStatus; }
    QString persistenceMessage() const { return m_persistenceMessage; }

    Q_INVOKABLE QString saveProfile(const QVariantMap &values);
    Q_INVOKABLE QString saveProfileWithCredential(const QVariantMap &values,
                                                  const QString &plaintext);
    Q_INVOKABLE bool removeProfile(const QString &id);
    Q_INVOKABLE bool setActiveProfile(const QString &id);
    Q_INVOKABLE bool setCredential(const QString &id, const QString &plaintext);
    Q_INVOKABLE bool clearCredential(const QString &id);
    Q_INVOKABLE bool hasCredential(const QString &id) const;
    Q_INVOKABLE QVariantMap profile(const QString &id) const;
    Q_INVOKABLE bool testConnection(const QString &id);
    Q_INVOKABLE bool refreshModels();
    Q_INVOKABLE bool setActiveModel(const QString &model);
    Q_INVOKABLE bool setActiveReasoningMode(const QString &mode);

    // Deliberately not invokable and not a Q_PROPERTY: only trusted C++ integration code can
    // obtain plaintext. The caller must clear the returned byte array after constructing headers.
    bool credential(const QString &id, QByteArray *plaintext, QString *errorMessage = nullptr) const;
    // Trusted data-management entry point. Clears every encrypted credential
    // with rollback if any credential file or settings persistence step fails.
    bool clearAllCredentials(QString *errorMessage = nullptr);
    QString settingsFilePath() const { return m_settingsFilePath; }
    QString credentialFilePath() const { return m_credentials.storageFilePath(); }
    QString settingsBackupFilePath() const { return m_settingsFilePath + QStringLiteral(".bak"); }

    static QString defaultSettingsFilePath();
    void setPrivacyConsentService(PrivacyConsentService *service);
#ifdef SMARTKEY_SETTINGS_TESTS
    static void setPersistFailureForTest(bool enabled);
#endif

    // Trusted C++ integration entry points. Discovery results never contain credentials.
    void setDiscoveredModels(const QString &profileId, const QStringList &models);
    void setModelDiscoveryError(const QString &profileId, const QString &message);

public slots:
    void setTestResult(const QString &status, const QString &message);
    void setStartupCompleted(bool completed);

signals:
    void profilesChanged();
    void activeProfileIdChanged();
    void configuredChanged();
    void testResultChanged();
    void activeSelectionChanged();
    void modelDiscoveryChanged();
    void startupCompletedChanged();
    void persistenceStatusChanged();
    void testConnectionRequested(const QString &id);
    void modelDiscoveryRequested(const QString &id);
    void operationFailed(const QString &operation, const QString &message);

private:
    struct LoadedState
    {
        QList<ProviderProfile> profiles;
        QString activeProfileId;
        bool startupCompleted = false;
        bool legacySchema = false;
    };

    int indexOf(const QString &id) const;
    QString saveProfileInternal(const QVariantMap &values, const QString &plaintext,
                                bool requireCompleteCredential);
    QByteArray credentialBindingFingerprint(const ProviderProfile &profile) const;
    bool hasBoundCredential(const ProviderProfile &profile) const;
    bool networkConsentGranted(const QString &action);
    bool parseSettingsFile(const QString &path, LoadedState *state,
                           QString *errorMessage) const;
    QByteArray serializedSettings(QString *errorMessage) const;
    bool writeFileAtomically(const QString &path, const QByteArray &contents,
                             QString *errorMessage) const;
    void applyLoadedState(const LoadedState &state);
    void setPersistenceStatus(const QString &status, const QString &message);
    bool persist();
    void load();
    void emitConfigurationChanges(bool wasConfigured, const QString &oldActiveProfileId);
    void resetModelDiscovery();

    QString m_settingsFilePath;
    CredentialStore m_credentials;
    QList<ProviderProfile> m_profiles;
    QString m_activeProfileId;
    QString m_lastTestStatus = QStringLiteral("idle");
    QString m_lastTestMessage;
    QStringList m_availableModels;
    QString m_modelDiscoveryMessage;
    bool m_modelDiscoveryInProgress = false;
    bool m_startupCompleted = false;
    PrivacyConsentService *m_privacyConsent = nullptr;
    QString m_persistenceStatus = QStringLiteral("ok");
    QString m_persistenceMessage;
};

} // namespace smartkey
