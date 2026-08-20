#pragma once

#include <QObject>
#include <QString>

namespace smartkey {

class PrivacyConsentService final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool consentGranted READ consentGranted NOTIFY consentChanged)
    Q_PROPERTY(bool consentRequired READ consentRequired NOTIFY consentChanged)
    Q_PROPERTY(QString policyVersion READ policyVersion CONSTANT)
    Q_PROPERTY(QString policyText READ policyText CONSTANT)
    Q_PROPERTY(QString acceptedAt READ acceptedAt NOTIFY consentChanged)
    Q_PROPERTY(QString acceptedLanguage READ acceptedLanguage NOTIFY consentChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    explicit PrivacyConsentService(const QString &settingsFilePath,
                                   const QString &policyVersion = currentPolicyVersion(),
                                   QObject *parent = nullptr);

    bool consentGranted() const { return m_consentGranted; }
    bool consentRequired() const { return !m_consentGranted; }
    QString policyVersion() const { return m_policyVersion; }
    QString policyText() const;
    QString acceptedAt() const { return m_acceptedAt; }
    QString acceptedLanguage() const { return m_acceptedLanguage; }
    QString lastError() const { return m_lastError; }
    QString settingsFilePath() const { return m_settingsFilePath; }

    Q_INVOKABLE bool accept(const QString &language);
    Q_INVOKABLE bool revoke();
    Q_INVOKABLE bool requireConsent(const QString &action);
    Q_INVOKABLE void requestPolicyDisplay();
    // This never deletes data. The UI must obtain explicit confirmation before
    // calling it; the application layer decides how a future deletion flow runs.
    Q_INVOKABLE void requestDataDeletion();

    static QString currentPolicyVersion();

signals:
    void consentChanged();
    void consentRequiredForAction(const QString &action);
    void policyDisplayRequested();
    void dataDeletionRequested();
    void lastErrorChanged();

private:
    bool persist(bool accepted, const QString &acceptedAt, const QString &language);
    void setLastError(const QString &message);
    void load();

    QString m_settingsFilePath;
    QString m_policyVersion;
    QString m_acceptedAt;
    QString m_acceptedLanguage;
    QString m_lastError;
    bool m_consentGranted = false;
};

} // namespace smartkey
