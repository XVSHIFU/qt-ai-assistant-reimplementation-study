#include "modeldiscoveryservice.h"

#include "urlpolicy.h"
#include "winhttptransport.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QUrl>

namespace smartkey {
namespace ai {
namespace {

constexpr int kMaximumModelsResponseBytes = 1024 * 1024;

AiError invalidRequest(const QString &message)
{
    AiError error;
    error.code = AiErrorCode::InvalidRequest;
    error.message = message;
    return error;
}

bool validHeaderName(const QByteArray &name)
{
    static const QRegularExpression pattern(
                QStringLiteral("^[!#$%&'*+.^_`|~0-9A-Za-z-]{1,64}$"));
    return pattern.match(QString::fromLatin1(name)).hasMatch();
}

} // namespace

ModelDiscoveryService::ModelDiscoveryService(QObject *parent)
    : QObject(parent), m_ownedTransport(new WinHttpTransport),
      m_transport(m_ownedTransport.data())
{
    initializeTransport();
}

ModelDiscoveryService::ModelDiscoveryService(AiHttpTransport *transport, QObject *parent)
    : QObject(parent), m_transport(transport)
{
    Q_ASSERT(transport);
    initializeTransport();
}

void ModelDiscoveryService::initializeTransport()
{
    if (!m_transport)
        return;
    connect(m_transport, &AiHttpTransport::responseStarted,
            this, &ModelDiscoveryService::onResponseStarted);
    connect(m_transport, &AiHttpTransport::dataReceived,
            this, &ModelDiscoveryService::onDataReceived);
    connect(m_transport, &AiHttpTransport::completed,
            this, &ModelDiscoveryService::onCompleted);
    connect(m_transport, &AiHttpTransport::failed,
            this, &ModelDiscoveryService::onFailed);
}

AiError ModelDiscoveryService::buildRequest(const QVariantMap &profile,
                                            const QByteArray &credential,
                                            AiHttpRequest *request)
{
    if (!request)
        return invalidRequest(QStringLiteral("The model discovery request output is null."));

    const QString providerType = profile.value(QStringLiteral("providerType")).toString().toLower();
    QUrl url(profile.value(QStringLiteral("baseUrl")).toString());
    if (providerType != QLatin1String("deepseek")
            && url.host().compare(QStringLiteral("api.deepseek.com"), Qt::CaseInsensitive) != 0) {
        return invalidRequest(QStringLiteral("Automatic model discovery is currently supported only for DeepSeek."));
    }

    url.setQuery(QString());
    url.setFragment(QString());
    QString path = url.path();
    while (path.endsWith(QLatin1Char('/')))
        path.chop(1);
    url.setPath(path + QStringLiteral("/models"));

    const bool allowLoopback = profile.value(QStringLiteral("allowLoopbackHttp"), false).toBool();
    const UrlPolicyResult policy = UrlPolicy::validateEndpoint(url, allowLoopback);
    if (!policy.accepted)
        return policy.error;

    AiHttpRequest result;
    result.url = policy.canonicalUrl;
    result.method = QByteArrayLiteral("GET");
    result.timeoutMs = qBound(1000, profile.value(QStringLiteral("timeoutMs"), 60000).toInt(),
                              300000);
    result.allowLoopbackHttp = allowLoopback;
    result.headers.insert(QByteArrayLiteral("accept"), QByteArrayLiteral("application/json"));

    const QString authScheme = profile.value(QStringLiteral("authScheme"),
                                              QStringLiteral("bearer")).toString().toLower();
    if (authScheme == QLatin1String("bearer")) {
        if (credential.isEmpty())
            return invalidRequest(QStringLiteral("The DeepSeek credential is missing."));
        result.headers.insert(QByteArrayLiteral("authorization"),
                              QByteArrayLiteral("Bearer ") + credential);
    } else if (authScheme == QLatin1String("api-key")) {
        const QByteArray header = profile.value(QStringLiteral("authHeaderName"),
                                                 QStringLiteral("api-key")).toByteArray();
        if (!validHeaderName(header) || credential.isEmpty())
            return invalidRequest(QStringLiteral("The API-key header or credential is invalid."));
        result.headers.insert(header.toLower(), credential);
    } else if (authScheme != QLatin1String("none")
               && authScheme != QLatin1String("anonymous")) {
        return invalidRequest(QStringLiteral("The configured authentication scheme is unsupported."));
    }

    *request = result;
    return {};
}

void ModelDiscoveryService::discover(const QString &profileId, const QVariantMap &profile,
                                     const QByteArray &credential)
{
    const QString normalizedProfileId = profileId.trimmed();
    if (normalizedProfileId.isEmpty()) {
        emit failed(profileId, invalidRequest(QStringLiteral("A profile ID is required for model discovery.")));
        return;
    }

    const quint64 generation = advanceGeneration(normalizedProfileId);
    AiHttpRequest request;
    const AiError validation = buildRequest(profile, credential, &request);
    if (validation.isError()) {
        if (m_latestGenerations.value(normalizedProfileId) == generation)
            emit failed(normalizedProfileId, validation);
        return;
    }

    const quint64 transportId = m_transport->start(request);
    if (transportId == 0) {
        AiError error;
        error.code = AiErrorCode::NetworkFailure;
        error.message = QStringLiteral("The model discovery transport could not start the request.");
        if (m_latestGenerations.value(normalizedProfileId) == generation)
            emit failed(normalizedProfileId, error);
        return;
    }
    RequestState state;
    state.profileId = normalizedProfileId;
    state.generation = generation;
    m_requests.insert(transportId, state);
    m_activeRequests.insert(normalizedProfileId, transportId);
}

void ModelDiscoveryService::cancelProfile(const QString &profileId)
{
    const QString normalizedProfileId = profileId.trimmed();
    if (normalizedProfileId.isEmpty())
        return;
    advanceGeneration(normalizedProfileId);
}

void ModelDiscoveryService::cancelAll()
{
    const QStringList profileIds = m_activeRequests.keys();
    for (const QString &profileId : profileIds)
        advanceGeneration(profileId);
}

quint64 ModelDiscoveryService::advanceGeneration(const QString &profileId)
{
    const quint64 generation = m_nextGeneration++;
    m_latestGenerations.insert(profileId, generation);

    const auto active = m_activeRequests.find(profileId);
    if (active != m_activeRequests.end()) {
        const quint64 transportId = active.value();
        m_activeRequests.erase(active);
        m_requests.remove(transportId);
        // State is removed first because some transports report cancellation
        // synchronously. The resulting callback is deliberately stale.
        m_transport->cancel(transportId);
    }
    return generation;
}

bool ModelDiscoveryService::isCurrent(quint64 transportId,
                                      const RequestState &state) const
{
    return m_latestGenerations.value(state.profileId) == state.generation
            && m_activeRequests.value(state.profileId) == transportId;
}

void ModelDiscoveryService::removeRequest(quint64 transportId,
                                          const RequestState &state)
{
    m_requests.remove(transportId);
    if (m_activeRequests.value(state.profileId) == transportId)
        m_activeRequests.remove(state.profileId);
}

void ModelDiscoveryService::onResponseStarted(quint64 transportId,
                                              const AiHttpResponseHead &head)
{
    auto it = m_requests.find(transportId);
    if (it != m_requests.end())
        it->statusCode = head.statusCode;
}

void ModelDiscoveryService::onDataReceived(quint64 transportId, const QByteArray &data)
{
    auto it = m_requests.find(transportId);
    if (it == m_requests.end())
        return;
    if (it->body.size() + data.size() > kMaximumModelsResponseBytes) {
        const RequestState state = it.value();
        const bool current = isCurrent(transportId, state);
        removeRequest(transportId, state);
        m_transport->cancel(transportId);
        if (!current)
            return;
        AiError error;
        error.code = AiErrorCode::ProtocolError;
        error.message = QStringLiteral("The model list response exceeded the safety limit.");
        emit failed(state.profileId, error);
        return;
    }
    it->body += data;
}

void ModelDiscoveryService::onCompleted(quint64 transportId)
{
    const auto it = m_requests.find(transportId);
    if (it == m_requests.end())
        return;
    const RequestState state = it.value();
    const bool current = isCurrent(transportId, state);
    removeRequest(transportId, state);
    if (!current)
        return;
    if (state.statusCode >= 400) {
        emit failed(state.profileId, httpError(state.statusCode));
        return;
    }
    QStringList models;
    const AiError parseError = parseModels(state.body, &models);
    if (parseError.isError()) {
        emit failed(state.profileId, parseError);
        return;
    }
    emit discovered(state.profileId, models);
}

void ModelDiscoveryService::onFailed(quint64 transportId, const AiError &error)
{
    const auto it = m_requests.find(transportId);
    if (it == m_requests.end())
        return;
    const RequestState state = it.value();
    const bool current = isCurrent(transportId, state);
    removeRequest(transportId, state);
    if (current)
        emit failed(state.profileId, error);
}

AiError ModelDiscoveryService::parseModels(const QByteArray &body, QStringList *models)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        AiError error;
        error.code = AiErrorCode::ParseError;
        error.message = QStringLiteral("DeepSeek returned a malformed model list.");
        return error;
    }

    QStringList result;
    QSet<QString> seen;
    const QJsonArray data = document.object().value(QStringLiteral("data")).toArray();
    for (const QJsonValue &value : data) {
        const QString id = value.toObject().value(QStringLiteral("id")).toString().trimmed();
        if (id.isEmpty() || id.size() > 128 || seen.contains(id))
            continue;
        seen.insert(id);
        result.append(id);
    }
    if (result.isEmpty()) {
        AiError error;
        error.code = AiErrorCode::ProtocolError;
        error.message = QStringLiteral("DeepSeek returned no available models.");
        return error;
    }
    if (models)
        *models = result;
    return {};
}

AiError ModelDiscoveryService::httpError(int statusCode)
{
    AiError error;
    error.httpStatus = statusCode;
    if (statusCode == 401 || statusCode == 403) {
        error.code = AiErrorCode::AuthenticationFailed;
        error.message = QStringLiteral("DeepSeek rejected the configured credential.");
    } else if (statusCode == 429) {
        error.code = AiErrorCode::RateLimited;
        error.message = QStringLiteral("DeepSeek rate-limited model discovery.");
        error.retryable = true;
    } else if (statusCode >= 400 && statusCode < 500) {
        error.code = AiErrorCode::HttpClientError;
        error.message = QStringLiteral("DeepSeek rejected the model-list request.");
    } else {
        error.code = AiErrorCode::HttpServerError;
        error.message = QStringLiteral("DeepSeek could not return its model list.");
        error.retryable = true;
    }
    return error;
}

} // namespace ai
} // namespace smartkey
