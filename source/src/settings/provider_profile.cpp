#include "provider_profile.h"

#include <QRegularExpression>
#include <QUrl>

namespace smartkey {
namespace {

QString trimmedOrDefault(const QVariantMap &map, const char *key, const QString &defaultValue)
{
    const QString value = map.value(QLatin1String(key)).toString().trimmed();
    return value.isEmpty() ? defaultValue : value;
}

int boundedTimeout(const QVariant &value)
{
    bool ok = false;
    const int timeout = value.toInt(&ok);
    return ok ? qBound(1000, timeout, 300000) : 60000;
}

int boundedLimit(const QVariant &value, int fallback, int minimum, int maximum)
{
    bool ok = false;
    const int parsed = value.toInt(&ok);
    return ok ? qBound(minimum, parsed, maximum) : fallback;
}

struct CapabilityDefinition
{
    const char *schema;
    bool supportsStreaming;
    bool supportsSearch;
    const char *searchRequestField;
};

// Capabilities are deliberately selected from this backend-owned registry.
// Values posted by QML (or left in an old settings file) cannot assert support
// for a request extension that the application has not reviewed.
const CapabilityDefinition kOpenAiChat = {
    "openai-chat-v1", true, false, ""
};
const CapabilityDefinition kOpenAiChatNonStreaming = {
    "openai-chat-nonstreaming-v1", false, false, ""
};
const CapabilityDefinition kDeepSeekChat = {
    "deepseek-chat-v1", true, false, ""
};

const CapabilityDefinition &capabilitiesFor(const QVariantMap &map,
                                             bool deepSeekProfile)
{
    if (deepSeekProfile)
        return kDeepSeekChat;

    const QString requested = map.value(QStringLiteral("capabilitySchema"))
            .toString().trimmed().toLower();
    if (requested == QLatin1String(kOpenAiChatNonStreaming.schema))
        return kOpenAiChatNonStreaming;
    return kOpenAiChat;
}

bool isSafeJsonField(const QString &field)
{
    static const QRegularExpression safeField(
                QStringLiteral("^[A-Za-z_][A-Za-z0-9_]{0,63}$"));
    return safeField.match(field).hasMatch();
}

} // namespace

ProviderProfile ProviderProfile::fromVariantMap(const QVariantMap &map, const QString &profileId)
{
    ProviderProfile profile;
    profile.id = profileId;
    profile.name = trimmedOrDefault(map, "name", QStringLiteral("Custom Provider"));
    profile.baseUrl = map.value(QStringLiteral("baseUrl")).toString().trimmed();
    while (profile.baseUrl.endsWith(QLatin1Char('/')))
        profile.baseUrl.chop(1);

    profile.chatPath = trimmedOrDefault(map, "chatPath", QStringLiteral("/v1/chat/completions"));
    if (!profile.chatPath.startsWith(QLatin1Char('/')))
        profile.chatPath.prepend(QLatin1Char('/'));
    profile.model = map.value(QStringLiteral("model")).toString().trimmed();
    const bool deepSeekEndpoint = QUrl(profile.baseUrl).host().compare(
                QStringLiteral("api.deepseek.com"), Qt::CaseInsensitive) == 0;
    profile.providerType = trimmedOrDefault(
                map, "providerType", deepSeekEndpoint ? QStringLiteral("deepseek")
                                                       : QStringLiteral("custom")).toLower();
    profile.apiStyle = trimmedOrDefault(map, "apiStyle", QStringLiteral("openai-chat"));
    profile.authScheme = trimmedOrDefault(map, "authScheme", QStringLiteral("bearer")).toLower();
    profile.authHeaderName = trimmedOrDefault(
        map, "authHeaderName",
        profile.authScheme == QLatin1String("api-key") ? QStringLiteral("api-key")
                                                       : QStringLiteral("Authorization"));
    profile.timeoutMs = boundedTimeout(map.value(QStringLiteral("timeoutMs")));
    profile.contextLimit = boundedLimit(map.value(QStringLiteral("contextLimit")),
                                        ProviderProfile::DefaultContextLimit,
                                        256, 2 * 1024 * 1024);
    profile.outputLimit = boundedLimit(map.value(QStringLiteral("outputLimit")),
                                       ProviderProfile::DefaultOutputLimit,
                                       1, 65536);
    // Always leave room for at least a minimal input message envelope.
    profile.outputLimit = qMin(profile.outputLimit, profile.contextLimit - 64);
    const CapabilityDefinition &capabilities = capabilitiesFor(
                map, deepSeekEndpoint || profile.providerType == QLatin1String("deepseek"));
    profile.capabilitySchema = QLatin1String(capabilities.schema);
    profile.supportsStreaming = capabilities.supportsStreaming;
    profile.supportsReasoning = map.value(QStringLiteral("supportsReasoning"), false).toBool();
    profile.supportsSearch = capabilities.supportsSearch;
    profile.searchRequestField = QLatin1String(capabilities.searchRequestField);
    profile.thinkingEnabled = map.value(QStringLiteral("thinkingEnabled"), deepSeekEndpoint).toBool();
    profile.reasoningEffort = trimmedOrDefault(map, "reasoningEffort", QStringLiteral("high")).toLower();
    if (profile.reasoningEffort == QLatin1String("medium"))
        profile.reasoningEffort = QStringLiteral("high");
    if (!QStringList{QStringLiteral("low"), QStringLiteral("high"),
                     QStringLiteral("xhigh"), QStringLiteral("max")}
            .contains(profile.reasoningEffort))
        profile.reasoningEffort = QStringLiteral("high");
    if (profile.thinkingEnabled)
        profile.supportsReasoning = true;
    return profile;
}

QVariantMap ProviderProfile::toVariantMap(bool hasStoredCredential) const
{
    return {
        {QStringLiteral("id"), id},
        {QStringLiteral("name"), name},
        {QStringLiteral("baseUrl"), baseUrl},
        {QStringLiteral("chatPath"), chatPath},
        {QStringLiteral("model"), model},
        {QStringLiteral("providerType"), providerType},
        {QStringLiteral("apiStyle"), apiStyle},
        {QStringLiteral("authScheme"), authScheme},
        {QStringLiteral("authHeaderName"), authHeaderName},
        {QStringLiteral("capabilitySchema"), capabilitySchema},
        {QStringLiteral("searchRequestField"), searchRequestField},
        {QStringLiteral("timeoutMs"), timeoutMs},
        {QStringLiteral("contextLimit"), contextLimit},
        {QStringLiteral("outputLimit"), outputLimit},
        {QStringLiteral("hasCredential"), hasStoredCredential},
        {QStringLiteral("supportsStreaming"), supportsStreaming},
        {QStringLiteral("supportsReasoning"), supportsReasoning},
        {QStringLiteral("supportsSearch"), supportsSearch},
        {QStringLiteral("thinkingEnabled"), thinkingEnabled},
        {QStringLiteral("reasoningEffort"), reasoningEffort}
    };
}

bool ProviderProfile::isValid(QString *errorMessage) const
{
    const QUrl url(baseUrl);
    if (id.isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Profile ID is empty");
        return false;
    }
    if (name.isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Profile name is empty");
        return false;
    }
    if (!url.isValid() || url.host().isEmpty()
        || (url.scheme() != QLatin1String("https") && url.scheme() != QLatin1String("http"))) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Base URL must be an absolute HTTP or HTTPS URL");
        return false;
    }
    if (model.isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Model is empty");
        return false;
    }
    if (chatPath.isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Chat path is empty");
        return false;
    }
    return true;
}

bool ProviderProfile::requiresCredential() const
{
    return authScheme != QLatin1String("none") && authScheme != QLatin1String("anonymous");
}

bool ProviderProfile::hasUsableSearchCapability() const
{
    return supportsSearch && !searchRequestField.isEmpty()
            && isSafeJsonField(searchRequestField);
}

} // namespace smartkey
