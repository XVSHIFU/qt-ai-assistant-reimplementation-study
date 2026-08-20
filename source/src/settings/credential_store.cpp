#include "credential_store.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>

#ifdef Q_OS_WIN
#  include <windows.h>
#  include <dpapi.h>
#endif

namespace smartkey {
namespace {

const char kCredentialGroup[] = "credentials";
const char kCredentialBindingGroup[] = "credentialBindings";
const char kEntropy[] = "SmartKeyAI.ProviderCredential.v1";
#ifdef SMARTKEY_SETTINGS_TESTS
bool g_forceProtectFailure = false;
#endif

QString windowsErrorMessage(unsigned long code)
{
#ifdef Q_OS_WIN
    wchar_t *buffer = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
                        | FORMAT_MESSAGE_IGNORE_INSERTS;
    const DWORD size = FormatMessageW(flags, nullptr, code, 0,
                                      reinterpret_cast<wchar_t *>(&buffer), 0, nullptr);
    const QString text = size && buffer
        ? QString::fromWCharArray(buffer, static_cast<int>(size)).trimmed()
        : QStringLiteral("Windows error %1").arg(code);
    if (buffer)
        LocalFree(buffer);
    return text;
#else
    Q_UNUSED(code)
    return QStringLiteral("Windows DPAPI is unavailable");
#endif
}

} // namespace

CredentialStore::CredentialStore(const QString &storageFilePath)
    : m_storageFilePath(storageFilePath.isEmpty() ? defaultStorageFilePath() : storageFilePath)
{
}

QString CredentialStore::defaultStorageFilePath()
{
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    return QDir(directory).filePath(QStringLiteral("credentials.ini"));
}

bool CredentialStore::validProfileId(const QString &profileId)
{
    static const QRegularExpression allowed(QStringLiteral("^[A-Za-z0-9_-]{1,64}$"));
    return allowed.match(profileId).hasMatch();
}

bool CredentialStore::setCredential(const QString &profileId, const QString &plaintext,
                                    QString *errorMessage)
{
    return setCredential(profileId, plaintext, QByteArray(), errorMessage);
}

bool CredentialStore::setCredential(const QString &profileId, const QString &plaintext,
                                    const QByteArray &bindingFingerprint,
                                    QString *errorMessage)
{
    if (!validProfileId(profileId)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Invalid profile ID");
        return false;
    }
    if (plaintext.isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Credential is empty; use clearCredential to remove it");
        return false;
    }

    QDir().mkpath(QFileInfo(m_storageFilePath).absolutePath());
    QByteArray utf8 = plaintext.toUtf8();
    const QByteArray protectedValue = protect(utf8, errorMessage);
    clearSensitive(&utf8);
    if (protectedValue.isEmpty())
        return false;

    return writeProtected(profileId, protectedValue, bindingFingerprint, errorMessage);
}

bool CredentialStore::credential(const QString &profileId, QByteArray *plaintext,
                                 QString *errorMessage) const
{
    return credential(profileId, QByteArray(), plaintext, errorMessage);
}

bool CredentialStore::credential(const QString &profileId,
                                 const QByteArray &expectedBindingFingerprint,
                                 QByteArray *plaintext, QString *errorMessage) const
{
    if (plaintext)
        clearSensitive(plaintext);
    if (!plaintext || !validProfileId(profileId)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Invalid credential request");
        return false;
    }

    QSettings settings(m_storageFilePath, QSettings::IniFormat);
    settings.beginGroup(QLatin1String(kCredentialGroup));
    const QByteArray encoded = settings.value(profileId).toByteArray();
    settings.endGroup();
    if (encoded.isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("No credential is stored for this profile");
        return false;
    }

    if (!expectedBindingFingerprint.isEmpty()) {
        settings.beginGroup(QLatin1String(kCredentialBindingGroup));
        const QByteArray storedFingerprint = settings.value(profileId).toByteArray();
        settings.endGroup();
        if (storedFingerprint.isEmpty()
            || storedFingerprint != expectedBindingFingerprint.toHex()) {
            if (errorMessage)
                *errorMessage = QStringLiteral("Credential is not bound to this provider endpoint");
            return false;
        }
    }

    const QByteArray decoded = QByteArray::fromBase64(encoded);
    QByteArray cleartext = unprotect(decoded, errorMessage);
    if (cleartext.isEmpty())
        return false;
    *plaintext = cleartext;
    clearSensitive(&cleartext);
    return true;
}

bool CredentialStore::hasCredential(const QString &profileId) const
{
    QByteArray plaintext;
    const bool present = credential(profileId, &plaintext, nullptr);
    clearSensitive(&plaintext);
    return present;
}

bool CredentialStore::hasCredential(const QString &profileId,
                                    const QByteArray &expectedBindingFingerprint) const
{
    QByteArray plaintext;
    const bool present = credential(profileId, expectedBindingFingerprint, &plaintext, nullptr);
    clearSensitive(&plaintext);
    return present;
}

bool CredentialStore::clearCredential(const QString &profileId, QString *errorMessage)
{
    if (!validProfileId(profileId)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Invalid profile ID");
        return false;
    }
    QSettings settings(m_storageFilePath, QSettings::IniFormat);
    settings.beginGroup(QLatin1String(kCredentialGroup));
    settings.remove(profileId);
    settings.endGroup();
    settings.beginGroup(QLatin1String(kCredentialBindingGroup));
    settings.remove(profileId);
    settings.endGroup();
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Failed to remove the credential");
        return false;
    }
    return true;
}

CredentialStore::Snapshot CredentialStore::snapshot(const QString &profileId) const
{
    Snapshot result;
    if (!validProfileId(profileId))
        return result;
    QSettings settings(m_storageFilePath, QSettings::IniFormat);
    settings.beginGroup(QLatin1String(kCredentialGroup));
    const QByteArray encoded = settings.value(profileId).toByteArray();
    settings.endGroup();
    if (encoded.isEmpty())
        return result;
    result.protectedValue = QByteArray::fromBase64(encoded);
    settings.beginGroup(QLatin1String(kCredentialBindingGroup));
    result.bindingFingerprint = QByteArray::fromHex(settings.value(profileId).toByteArray());
    settings.endGroup();
    result.existed = true;
    return result;
}

bool CredentialStore::restore(const QString &profileId, const Snapshot &value,
                              QString *errorMessage)
{
    if (!value.existed)
        return clearCredential(profileId, errorMessage);
    return writeProtected(profileId, value.protectedValue, value.bindingFingerprint, errorMessage);
}

bool CredentialStore::writeProtected(const QString &profileId,
                                     const QByteArray &protectedValue,
                                     const QByteArray &bindingFingerprint,
                                     QString *errorMessage)
{
    if (!validProfileId(profileId) || protectedValue.isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Invalid protected credential");
        return false;
    }

    QDir().mkpath(QFileInfo(m_storageFilePath).absolutePath());
    QSettings settings(m_storageFilePath, QSettings::IniFormat);
    settings.beginGroup(QLatin1String(kCredentialGroup));
    settings.setValue(profileId, QString::fromLatin1(protectedValue.toBase64()));
    settings.endGroup();
    settings.beginGroup(QLatin1String(kCredentialBindingGroup));
    if (bindingFingerprint.isEmpty())
        settings.remove(profileId);
    else
        settings.setValue(profileId, QString::fromLatin1(bindingFingerprint.toHex()));
    settings.endGroup();
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Failed to persist the protected credential");
        return false;
    }
    return true;
}

void CredentialStore::clearSensitive(QByteArray *value)
{
    if (!value)
        return;
    if (!value->isEmpty()) {
#ifdef Q_OS_WIN
        SecureZeroMemory(value->data(), static_cast<SIZE_T>(value->size()));
#else
        volatile char *bytes = value->data();
        for (int i = 0; i < value->size(); ++i)
            bytes[i] = 0;
#endif
    }
    value->clear();
}

#ifdef SMARTKEY_SETTINGS_TESTS
void CredentialStore::setProtectFailureForTest(bool enabled)
{
    g_forceProtectFailure = enabled;
}
#endif

QByteArray CredentialStore::protect(const QByteArray &plaintext, QString *errorMessage)
{
#ifdef SMARTKEY_SETTINGS_TESTS
    if (g_forceProtectFailure) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Forced DPAPI protection failure");
        return {};
    }
#endif
#ifdef Q_OS_WIN
    DATA_BLOB input;
    input.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(plaintext.constData()));
    input.cbData = static_cast<DWORD>(plaintext.size());
    DATA_BLOB entropy;
    entropy.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(kEntropy));
    entropy.cbData = static_cast<DWORD>(sizeof(kEntropy) - 1);
    DATA_BLOB output = {0, nullptr};
    if (!CryptProtectData(&input, L"SmartKey AI provider credential", &entropy, nullptr, nullptr,
                          CRYPTPROTECT_UI_FORBIDDEN, &output)) {
        if (errorMessage)
            *errorMessage = windowsErrorMessage(GetLastError());
        return {};
    }
    const QByteArray result(reinterpret_cast<const char *>(output.pbData),
                            static_cast<int>(output.cbData));
    SecureZeroMemory(output.pbData, output.cbData);
    LocalFree(output.pbData);
    return result;
#else
    Q_UNUSED(plaintext)
    if (errorMessage)
        *errorMessage = QStringLiteral("DPAPI credential storage is supported only on Windows");
    return {};
#endif
}

QByteArray CredentialStore::unprotect(const QByteArray &ciphertext, QString *errorMessage)
{
#ifdef Q_OS_WIN
    DATA_BLOB input;
    input.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(ciphertext.constData()));
    input.cbData = static_cast<DWORD>(ciphertext.size());
    DATA_BLOB entropy;
    entropy.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(kEntropy));
    entropy.cbData = static_cast<DWORD>(sizeof(kEntropy) - 1);
    DATA_BLOB output = {0, nullptr};
    if (!CryptUnprotectData(&input, nullptr, &entropy, nullptr, nullptr,
                            CRYPTPROTECT_UI_FORBIDDEN, &output)) {
        if (errorMessage)
            *errorMessage = windowsErrorMessage(GetLastError());
        return {};
    }
    const QByteArray result(reinterpret_cast<const char *>(output.pbData),
                            static_cast<int>(output.cbData));
    SecureZeroMemory(output.pbData, output.cbData);
    LocalFree(output.pbData);
    return result;
#else
    Q_UNUSED(ciphertext)
    if (errorMessage)
        *errorMessage = QStringLiteral("DPAPI credential storage is supported only on Windows");
    return {};
#endif
}

} // namespace smartkey
