#include "ai/openaicompatibleprovider.h"
#include "ai/modeldiscoveryservice.h"
#include "settings/provider_settings.h"

#include <QCoreApplication>
#include <QTextStream>
#include <QTimer>

using namespace smartkey::ai;

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("SmartKeyAI"));
    app.setApplicationName(QStringLiteral("SmartKey AI"));

    QByteArray key = qgetenv("DEEPSEEK_API_KEY");
    if (key.isEmpty()) {
        smartkey::ProviderSettings settings;
        const QString activeId = settings.activeProfileId();
        const QVariantMap active = settings.profile(activeId);
        QString credentialError;
        if (!active.value(QStringLiteral("baseUrl")).toString().contains(
                    QStringLiteral("api.deepseek.com"), Qt::CaseInsensitive)
                || !settings.credential(activeId, &key, &credentialError)) {
            QTextStream(stderr) << "LIVE_SMOKE=FAIL reason=missing_deepseek_credential\n";
            return 2;
        }
    }

    OpenAiCompatibleProvider provider;
    ModelDiscoveryService discovery;
    AiEndpointConfig config;
    config.endpoint = QUrl(QStringLiteral("https://api.deepseek.com/chat/completions"));
    config.defaultModel = QStringLiteral("deepseek-v4-flash");
    config.authMode = AiAuthMode::Bearer;
    config.apiKey = key;
    config.timeoutMs = 40000;
    AiError configurationError;
    if (!provider.configure(config, &configurationError)) {
        key.fill('\0');
        config.apiKey.fill('\0');
        QTextStream(stderr) << "LIVE_SMOKE=FAIL reason=configuration code="
                            << static_cast<int>(configurationError.code) << "\n";
        return 3;
    }
    QByteArray discoveryCredential = key;
    key.fill('\0');
    key.clear();
    config.apiKey.fill('\0');
    config.apiKey.clear();

    QString content;
    QString reasoning;
    bool modelDiscovered = false;
    QString requestId;
    QObject::connect(&provider, &OpenAiCompatibleProvider::contentDelta,
                     &app, [&](const QString &, const QString &delta) { content += delta; });
    QObject::connect(&provider, &OpenAiCompatibleProvider::reasoningDelta,
                     &app, [&](const QString &, const QString &delta) { reasoning += delta; });
    QObject::connect(&provider, &OpenAiCompatibleProvider::completed,
                     &app, [&](const QString &) {
        const bool ok = modelDiscovered && !content.trimmed().isEmpty()
                && !reasoning.trimmed().isEmpty();
        QTextStream(stdout) << "LIVE_SMOKE=" << (ok ? "PASS" : "FAIL")
                            << " content_nonempty=" << (!content.trimmed().isEmpty() ? 1 : 0)
                            << " reasoning_nonempty=" << (!reasoning.trimmed().isEmpty() ? 1 : 0)
                            << " configured_model_discovered=" << (modelDiscovered ? 1 : 0)
                            << "\n";
        app.exit(ok ? 0 : 5);
    });
    QObject::connect(&provider, &OpenAiCompatibleProvider::failed,
                     &app, [&](const QString &, const AiError &error) {
        QTextStream(stderr) << "LIVE_SMOKE=FAIL code=" << static_cast<int>(error.code)
                            << " http_status=" << error.httpStatus << "\n";
        app.exit(6);
    });

    AiRequest request;
    request.model = QStringLiteral("deepseek-v4-flash");
    request.stream = false;
    request.reasoningEnabled = true;
    request.messages.append({QStringLiteral("system"), QStringLiteral("You are a helpful assistant.")});
    request.messages.append({QStringLiteral("user"), QStringLiteral("Reply with exactly: OK")});
    request.additionalBody.insert(QStringLiteral("thinking"),
                                  QVariantMap{{QStringLiteral("type"), QStringLiteral("enabled")}});
    request.additionalBody.insert(QStringLiteral("reasoning_effort"), QStringLiteral("high"));
    request.additionalBody.insert(QStringLiteral("max_tokens"), 64);
    QObject::connect(&discovery, &ModelDiscoveryService::discovered,
                     &app, [&](const QString &, const QStringList &models) {
        modelDiscovered = models.contains(QStringLiteral("deepseek-v4-flash"));
        if (!modelDiscovered) {
            QTextStream(stderr) << "LIVE_SMOKE=FAIL reason=configured_model_not_discovered\n";
            app.exit(8);
            return;
        }
        requestId = provider.start(request);
    });
    QObject::connect(&discovery, &ModelDiscoveryService::failed,
                     &app, [&](const QString &, const AiError &error) {
        QTextStream(stderr) << "LIVE_SMOKE=FAIL reason=model_discovery code="
                            << static_cast<int>(error.code)
                            << " http_status=" << error.httpStatus << "\n";
        app.exit(9);
    });
    discovery.discover(
                QStringLiteral("live-deepseek"),
                {{QStringLiteral("providerType"), QStringLiteral("deepseek")},
                 {QStringLiteral("baseUrl"), QStringLiteral("https://api.deepseek.com")},
                 {QStringLiteral("authScheme"), QStringLiteral("bearer")},
                 {QStringLiteral("timeoutMs"), 40000}},
                discoveryCredential);
    discoveryCredential.fill('\0');
    discoveryCredential.clear();
    QTimer::singleShot(45000, &app, [&] {
        if (!requestId.isEmpty())
            provider.cancel(requestId);
        QTextStream(stderr) << "LIVE_SMOKE=FAIL reason=timeout\n";
        app.exit(7);
    });
    return app.exec();
}
