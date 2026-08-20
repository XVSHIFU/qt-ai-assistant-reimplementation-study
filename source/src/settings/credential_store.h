#pragma once

#include <QByteArray>
#include <QString>

namespace smartkey {

class ProviderSettings;

class CredentialStore
{
public:
    explicit CredentialStore(const QString &storageFilePath = QString());

    QString storageFilePath() const { return m_storageFilePath; }
    bool setCredential(const QString &profileId, const QString &plaintext, QString *errorMessage = nullptr);
    bool credential(const QString &profileId, QByteArray *plaintext, QString *errorMessage = nullptr) const;
    bool hasCredential(const QString &profileId) const;
    bool clearCredential(const QString &profileId, QString *errorMessage = nullptr);

    static void clearSensitive(QByteArray *value);
    static QString defaultStorageFilePath();
#ifdef SMARTKEY_SETTINGS_TESTS
    static void setProtectFailureForTest(bool enabled);
#endif

private:
    struct Snapshot
    {
        QByteArray protectedValue;
        QByteArray bindingFingerprint;
        bool existed = false;
    };

    friend class ProviderSettings;

    bool setCredential(const QString &profileId, const QString &plaintext,
                       const QByteArray &bindingFingerprint,
                       QString *errorMessage = nullptr);
    bool credential(const QString &profileId, const QByteArray &expectedBindingFingerprint,
                    QByteArray *plaintext, QString *errorMessage = nullptr) const;
    bool hasCredential(const QString &profileId,
                       const QByteArray &expectedBindingFingerprint) const;
    Snapshot snapshot(const QString &profileId) const;
    bool restore(const QString &profileId, const Snapshot &snapshot,
                 QString *errorMessage = nullptr);
    bool writeProtected(const QString &profileId, const QByteArray &protectedValue,
                        const QByteArray &bindingFingerprint, QString *errorMessage);

    static bool validProfileId(const QString &profileId);
    static QByteArray protect(const QByteArray &plaintext, QString *errorMessage);
    static QByteArray unprotect(const QByteArray &ciphertext, QString *errorMessage);

    QString m_storageFilePath;
};

} // namespace smartkey
