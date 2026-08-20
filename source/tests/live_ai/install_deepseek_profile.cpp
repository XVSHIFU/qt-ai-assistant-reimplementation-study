#include "settings/credential_store.h"
#include "settings/provider_settings.h"

#include <QCoreApplication>
#include <QTextStream>

using namespace smartkey;

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("SmartKeyAI"));
    app.setApplicationName(QStringLiteral("SmartKey AI"));

    QByteArray credentialBytes = qgetenv("DEEPSEEK_API_KEY");
    if (credentialBytes.isEmpty()) {
        QTextStream(stderr) << "INSTALL_PROFILE=FAIL reason=missing_environment_credential\n";
        return 2;
    }

    ProviderSettings settings;
    QString existingId;
    const QVariantMap current = settings.profile(settings.activeProfileId());
    if (current.value(QStringLiteral("baseUrl")).toString().contains(
                QStringLiteral("api.deepseek.com"), Qt::CaseInsensitive))
        existingId = settings.activeProfileId();

    const QVariantMap values = {
        {QStringLiteral("id"), existingId},
        {QStringLiteral("name"), QStringLiteral("DeepSeek")},
        {QStringLiteral("providerType"), QStringLiteral("deepseek")},
        {QStringLiteral("baseUrl"), QStringLiteral("https://api.deepseek.com")},
        {QStringLiteral("chatPath"), QStringLiteral("/chat/completions")},
        {QStringLiteral("model"), QStringLiteral("deepseek-v4-flash")},
        {QStringLiteral("authScheme"), QStringLiteral("bearer")},
        {QStringLiteral("authHeaderName"), QStringLiteral("Authorization")},
        {QStringLiteral("timeoutMs"), 60000},
        {QStringLiteral("supportsStreaming"), true},
        {QStringLiteral("supportsReasoning"), true},
        {QStringLiteral("supportsSearch"), false},
        {QStringLiteral("thinkingEnabled"), true},
        {QStringLiteral("reasoningEffort"), QStringLiteral("high")}
    };
    const QString id = settings.saveProfile(values);
    if (id.isEmpty()) {
        credentialBytes.fill('\0');
        QTextStream(stderr) << "INSTALL_PROFILE=FAIL reason=save_profile\n";
        return 3;
    }

    QString credential = QString::fromUtf8(credentialBytes);
    credentialBytes.fill('\0');
    credentialBytes.clear();
    const bool credentialSaved = settings.setCredential(id, credential);
    credential.fill(QChar('\0'));
    credential.clear();
    if (!credentialSaved || !settings.setActiveProfile(id)) {
        QTextStream(stderr) << "INSTALL_PROFILE=FAIL reason=dpapi_or_activation\n";
        return 4;
    }
    settings.setStartupCompleted(true);
    QTextStream(stdout) << "INSTALL_PROFILE=PASS configured="
                        << (settings.configured() ? 1 : 0)
                        << " has_credential=" << (settings.hasCredential(id) ? 1 : 0) << "\n";
    return settings.configured() ? 0 : 5;
}
