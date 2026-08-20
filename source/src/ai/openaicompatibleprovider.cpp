#include "openaicompatibleprovider.h"

#include "urlpolicy.h"
#include "winhttptransport.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QTimer>
#include <QUuid>

namespace smartkey {
namespace ai {

namespace {

const int MaximumBufferedBody = 4 * 1024 * 1024;

bool isHeaderName(const QByteArray &name)
{
    static const QRegularExpression expression(
                QStringLiteral("^[!#$%&'*+.^_`|~0-9A-Za-z-]+$"));
    return !name.isEmpty() && expression.match(QString::fromLatin1(name)).hasMatch();
}

bool isJsonFieldName(const QByteArray &name)
{
    static const QRegularExpression expression(QStringLiteral("^[A-Za-z_][A-Za-z0-9_.-]{0,127}$"));
    return name.isEmpty() || expression.match(QString::fromLatin1(name)).hasMatch();
}

bool isSafeHeaderValue(const QByteArray &value)
{
    return !value.contains('\r') && !value.contains('\n') && !value.contains('\0');
}

bool isReservedHeader(const QByteArray &name)
{
    const QByteArray lower = name.toLower();
    return lower == "authorization" || lower == "proxy-authorization"
            || lower == "host" || lower == "content-length"
            || lower == "transfer-encoding" || lower == "connection"
            || lower == "content-type" || lower == "accept";
}

AiError invalidRequest(const QString &message)
{
    AiError error;
    error.code = AiErrorCode::InvalidRequest;
    error.message = message;
    return error;
}

QString safeProviderToken(const QJsonValue &value)
{
    const QString token = value.isString() ? value.toString().trimmed() : QString();
    static const QRegularExpression safeToken(
                QStringLiteral("^[A-Za-z0-9][A-Za-z0-9_.:-]{0,63}$"));
    return safeToken.match(token).hasMatch() ? token : QString();
}

void readSafeProviderMetadata(const QByteArray &body, AiError *error)
{
    if (!error || body.isEmpty())
        return;
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return;
    QJsonObject object = document.object();
    if (object.value(QStringLiteral("error")).isObject())
        object = object.value(QStringLiteral("error")).toObject();
    error->providerCode = safeProviderToken(object.value(QStringLiteral("code")));
    error->providerType = safeProviderToken(object.value(QStringLiteral("type")));
}

int retryAfterSeconds(const QMap<QByteArray, QByteArray> &headers)
{
    const QByteArray value = headers.value(QByteArrayLiteral("retry-after")).trimmed();
    bool ok = false;
    const int seconds = value.toInt(&ok);
    return ok && seconds >= 0 && seconds <= 24 * 60 * 60 ? seconds : -1;
}

} // namespace

OpenAiCompatibleProvider::OpenAiCompatibleProvider(AiHttpTransport *transport, QObject *parent)
    : QObject(parent), m_transport(transport)
{
    qRegisterMetaType<AiError>();
    if (!m_transport) {
        m_transport = new WinHttpTransport(this);
    }
    connect(m_transport, &AiHttpTransport::responseStarted,
            this, &OpenAiCompatibleProvider::onResponseStarted);
    connect(m_transport, &AiHttpTransport::dataReceived,
            this, &OpenAiCompatibleProvider::onDataReceived);
    connect(m_transport, &AiHttpTransport::completed,
            this, &OpenAiCompatibleProvider::onTransportCompleted);
    connect(m_transport, &AiHttpTransport::failed,
            this, &OpenAiCompatibleProvider::onTransportFailed);
}

OpenAiCompatibleProvider::~OpenAiCompatibleProvider()
{
    disconnect(m_transport, nullptr, this, nullptr);
    const QList<quint64> ids = m_requests.keys();
    m_requests.clear();
    m_publicToTransport.clear();
    for (quint64 id : ids)
        m_transport->cancel(id);
    secureClear(&m_config.apiKey);
}

void OpenAiCompatibleProvider::secureClear(QByteArray *value)
{
    if (!value)
        return;
    value->fill('\0');
    value->clear();
}

AiError OpenAiCompatibleProvider::validateConfig(const AiEndpointConfig &config)
{
    const UrlPolicyResult policy = UrlPolicy::validateEndpoint(config.endpoint,
                                                                config.allowLoopbackHttp);
    if (!policy.accepted)
        return policy.error;
    if (config.defaultModel.trimmed().isEmpty())
        return invalidRequest(QStringLiteral("A model name is required."));
    if (config.timeoutMs < 1000 || config.timeoutMs > 10 * 60 * 1000)
        return invalidRequest(QStringLiteral("The timeout must be between 1 second and 10 minutes."));
    if (config.authMode != AiAuthMode::None && config.apiKey.isEmpty())
        return invalidRequest(QStringLiteral("An API credential is required."));
    if (!isSafeHeaderValue(config.apiKey))
        return invalidRequest(QStringLiteral("The API credential contains invalid characters."));
    if (config.authMode == AiAuthMode::Header
            && (!isHeaderName(config.apiKeyHeader) || isReservedHeader(config.apiKeyHeader)))
        return invalidRequest(QStringLiteral("The API credential header name is not allowed."));
    if (!isJsonFieldName(config.reasoningRequestField)
            || !isJsonFieldName(config.searchRequestField))
        return invalidRequest(QStringLiteral("A configured capability field name is invalid."));

    for (auto it = config.additionalHeaders.constBegin(); it != config.additionalHeaders.constEnd(); ++it) {
        if (!isHeaderName(it.key()) || isReservedHeader(it.key()) || !isSafeHeaderValue(it.value()))
            return invalidRequest(QStringLiteral("An additional HTTP header is invalid or reserved."));
    }
    static const QStringList reservedBody = {
        QStringLiteral("model"), QStringLiteral("messages"), QStringLiteral("stream")
    };
    for (const QString &key : reservedBody) {
        if (config.additionalBody.contains(key))
            return invalidRequest(QStringLiteral("Additional request data may not replace model, messages, or stream."));
    }
    return AiError();
}

bool OpenAiCompatibleProvider::configure(const AiEndpointConfig &config, AiError *error)
{
    const AiError validation = validateConfig(config);
    if (validation.isError()) {
        if (error)
            *error = validation;
        return false;
    }
    secureClear(&m_config.apiKey);
    m_config = config;
    m_config.endpoint = UrlPolicy::validateEndpoint(config.endpoint,
                                                     config.allowLoopbackHttp).canonicalUrl;
    if (error)
        *error = AiError();
    return true;
}

AiError OpenAiCompatibleProvider::buildHttpRequest(const AiEndpointConfig &config,
                                                   const AiRequest &request,
                                                   AiHttpRequest *httpRequest)
{
    const AiError configError = validateConfig(config);
    if (configError.isError())
        return configError;
    if (!httpRequest)
        return invalidRequest(QStringLiteral("No HTTP request destination was supplied."));
    if (request.messages.isEmpty())
        return invalidRequest(QStringLiteral("At least one chat message is required."));

    const QString model = request.model.trimmed().isEmpty()
            ? config.defaultModel.trimmed() : request.model.trimmed();
    if (model.isEmpty())
        return invalidRequest(QStringLiteral("A model name is required."));

    QJsonArray messages;
    static const QStringList roles = {
        QStringLiteral("system"), QStringLiteral("developer"), QStringLiteral("user"),
        QStringLiteral("assistant"), QStringLiteral("tool")
    };
    for (const AiMessage &message : request.messages) {
        const QString role = message.role.trimmed().toLower();
        if (!roles.contains(role))
            return invalidRequest(QStringLiteral("A chat message has an unsupported role."));
        QJsonObject item;
        item.insert(QStringLiteral("role"), role);
        item.insert(QStringLiteral("content"), message.content);
        messages.append(item);
    }

    QJsonObject body = QJsonObject::fromVariantMap(config.additionalBody);
    static const QStringList requestReservedBody = {
        QStringLiteral("model"), QStringLiteral("messages"), QStringLiteral("stream")
    };
    for (auto it = request.additionalBody.constBegin(); it != request.additionalBody.constEnd(); ++it) {
        if (requestReservedBody.contains(it.key()))
            return invalidRequest(QStringLiteral("Request options may not replace model, messages, or stream."));
        body.insert(it.key(), QJsonValue::fromVariant(it.value()));
    }
    body.insert(QStringLiteral("model"), model);
    body.insert(QStringLiteral("messages"), messages);
    body.insert(QStringLiteral("stream"), request.stream);
    if (request.reasoningEnabled && !config.reasoningRequestField.isEmpty())
        body.insert(QString::fromLatin1(config.reasoningRequestField), true);
    if (request.searchEnabled && !config.searchRequestField.isEmpty())
        body.insert(QString::fromLatin1(config.searchRequestField), true);

    httpRequest->url = config.endpoint;
    httpRequest->method = QByteArrayLiteral("POST");
    httpRequest->headers.clear();
    httpRequest->headers.insert(QByteArrayLiteral("content-type"), QByteArrayLiteral("application/json"));
    httpRequest->headers.insert(QByteArrayLiteral("accept"), request.stream
                                ? QByteArrayLiteral("text/event-stream")
                                : QByteArrayLiteral("application/json"));
    for (auto it = config.additionalHeaders.constBegin(); it != config.additionalHeaders.constEnd(); ++it)
        httpRequest->headers.insert(it.key(), it.value());
    if (config.authMode == AiAuthMode::Bearer)
        httpRequest->headers.insert(QByteArrayLiteral("authorization"),
                                    QByteArrayLiteral("Bearer ") + config.apiKey);
    else if (config.authMode == AiAuthMode::Header)
        httpRequest->headers.insert(config.apiKeyHeader, config.apiKey);
    httpRequest->body = QJsonDocument(body).toJson(QJsonDocument::Compact);
    httpRequest->timeoutMs = config.timeoutMs;
    httpRequest->allowLoopbackHttp = config.allowLoopbackHttp;
    return AiError();
}

QString OpenAiCompatibleProvider::start(const AiRequest &request)
{
    const QString publicId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    AiHttpRequest httpRequest;
    const AiError error = buildHttpRequest(m_config, request, &httpRequest);
    if (error.isError()) {
        QTimer::singleShot(0, this, [this, publicId, error] { emit failed(publicId, error); });
        return publicId;
    }

    QSharedPointer<RequestState> state(new RequestState);
    state->publicId = publicId;
    state->request = request;
    state->allowSseEofCompletion = m_config.allowSseEofCompletion;
    const quint64 transportId = m_transport->start(httpRequest);
    if (transportId == 0) {
        AiError startError;
        startError.code = AiErrorCode::NetworkFailure;
        startError.message = QStringLiteral("The HTTP transport could not start the request.");
        QTimer::singleShot(0, this, [this, publicId, startError] {
            emit failed(publicId, startError);
        });
        return publicId;
    }
    m_requests.insert(transportId, state);
    m_publicToTransport.insert(publicId, transportId);
    return publicId;
}

void OpenAiCompatibleProvider::cancel(const QString &requestId)
{
    const auto it = m_publicToTransport.constFind(requestId);
    if (it != m_publicToTransport.constEnd())
        m_transport->cancel(it.value());
}

void OpenAiCompatibleProvider::cancelAll()
{
    const QList<quint64> ids = m_requests.keys();
    for (quint64 id : ids)
        m_transport->cancel(id);
}

void OpenAiCompatibleProvider::onResponseStarted(quint64 transportId,
                                                 const AiHttpResponseHead &head)
{
    const QSharedPointer<RequestState> state = m_requests.value(transportId);
    if (!state)
        return;
    state->statusCode = head.statusCode;
    state->responseHeaders = head.headers;
    const QByteArray contentType = head.headers.value(QByteArrayLiteral("content-type")).toLower();
    state->usesSse = state->request.stream
            && (contentType.isEmpty() || contentType.contains("text/event-stream"));
}

void OpenAiCompatibleProvider::onDataReceived(quint64 transportId, const QByteArray &data)
{
    const QSharedPointer<RequestState> state = m_requests.value(transportId);
    if (!state)
        return;
    if (state->statusCode >= 400 || !state->usesSse) {
        if (state->body.size() + data.size() > MaximumBufferedBody) {
            AiError error;
            error.code = AiErrorCode::ProtocolError;
            error.message = QStringLiteral("The API response exceeded the configured size limit.");
            failState(transportId, state, error, true);
            return;
        }
        state->body.append(data);
        return;
    }
    const QList<SseEvent> events = state->parser.feed(data);
    if (state->parser.hasError()) {
        failState(transportId, state, state->parser.error(), true);
        return;
    }
    consumeSseEvents(transportId, state, events);
}

bool OpenAiCompatibleProvider::consumeSseEvents(quint64 transportId,
                                                const QSharedPointer<RequestState> &state,
                                                const QList<SseEvent> &events)
{
    for (const SseEvent &event : events) {
        state->sawSseEvent = true;
        if (event.data.trimmed() == QByteArrayLiteral("[DONE]")) {
            state->sawDone = true;
            continue;
        }
        if (!consumeJsonPayload(transportId, state, event.data, true))
            return false;
    }
    return true;
}

QString OpenAiCompatibleProvider::jsonText(const QJsonValue &value)
{
    if (value.isString())
        return value.toString();
    if (value.isArray()) {
        QStringList parts;
        for (const QJsonValue &item : value.toArray()) {
            if (item.isString()) {
                parts.append(item.toString());
            } else if (item.isObject()) {
                const QJsonObject object = item.toObject();
                const QString title = object.value(QStringLiteral("title")).toString();
                const QString url = object.value(QStringLiteral("url")).toString();
                if (!url.isEmpty())
                    parts.append(title.isEmpty() ? url : QStringLiteral("[%1](%2)").arg(title, url));
            }
        }
        return parts.join(QLatin1Char('\n'));
    }
    return QString();
}

bool OpenAiCompatibleProvider::consumeJsonPayload(quint64 transportId,
                                                  const QSharedPointer<RequestState> &state,
                                                  const QByteArray &payload,
                                                  bool streaming)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        AiError error;
        error.code = AiErrorCode::ParseError;
        error.message = QStringLiteral("The API returned malformed JSON.");
        failState(transportId, state, error, true);
        return false;
    }
    const QJsonObject root = document.object();
    if (root.contains(QStringLiteral("error"))) {
        AiError error;
        error.code = AiErrorCode::ProtocolError;
        error.message = QStringLiteral("The API returned an error response.");
        failState(transportId, state, error, true);
        return false;
    }
    if (root.value(QStringLiteral("usage")).isObject())
        emit usage(state->publicId, root.value(QStringLiteral("usage")).toObject().toVariantMap());

    const QJsonArray choices = root.value(QStringLiteral("choices")).toArray();
    if (choices.isEmpty() || !choices.first().isObject())
        return true;
    const QJsonObject choice = choices.first().toObject();
    if (streaming) {
        const QJsonValue finishReason = choice.value(QStringLiteral("finish_reason"));
        if (finishReason.isString() && !finishReason.toString().trimmed().isEmpty())
            state->sawFinishReason = true;
    }
    const QJsonObject value = streaming
            ? choice.value(QStringLiteral("delta")).toObject()
            : choice.value(QStringLiteral("message")).toObject();

    const QString content = jsonText(value.value(QStringLiteral("content")));
    if (!content.isEmpty())
        emit contentDelta(state->publicId, content);
    QString reasoning = jsonText(value.value(QStringLiteral("reasoning_content")));
    if (reasoning.isEmpty())
        reasoning = jsonText(value.value(QStringLiteral("reasoning")));
    if (!reasoning.isEmpty())
        emit reasoningDelta(state->publicId, reasoning);
    QString reference = jsonText(value.value(QStringLiteral("reference")));
    if (reference.isEmpty())
        reference = jsonText(value.value(QStringLiteral("references")));
    if (reference.isEmpty())
        reference = jsonText(root.value(QStringLiteral("reference")));
    if (reference.isEmpty())
        reference = jsonText(root.value(QStringLiteral("references")));
    if (!reference.isEmpty())
        emit referenceDelta(state->publicId, reference);
    return true;
}

void OpenAiCompatibleProvider::onTransportCompleted(quint64 transportId)
{
    const QSharedPointer<RequestState> state = m_requests.value(transportId);
    if (!state)
        return;
    if (state->statusCode >= 400) {
        failState(transportId, state,
                  httpError(state->statusCode, state->responseHeaders, state->body), false);
        return;
    }
    if (state->usesSse) {
        if (!consumeSseEvents(transportId, state, state->parser.finish()))
            return;
        if (state->parser.hasError()) {
            failState(transportId, state, state->parser.error(), false);
            return;
        }
        if (!state->sawSseEvent) {
            AiError error;
            error.code = AiErrorCode::ProtocolError;
            error.message = QStringLiteral("The API returned an empty streaming response.");
            failState(transportId, state, error, false);
            return;
        }
        if (!state->sawDone && !state->sawFinishReason
                && !state->allowSseEofCompletion) {
            AiError error;
            error.code = AiErrorCode::ProtocolError;
            error.message = QStringLiteral(
                    "The streaming response ended before a terminal event.");
            error.retryable = true;
            failState(transportId, state, error, false);
            return;
        }
    } else if (!consumeJsonPayload(transportId, state, state->body, false)) {
        return;
    }
    finishState(transportId, state);
}

void OpenAiCompatibleProvider::onTransportFailed(quint64 transportId, const AiError &error)
{
    const QSharedPointer<RequestState> state = m_requests.value(transportId);
    if (state)
        failState(transportId, state, error, false);
}

void OpenAiCompatibleProvider::finishState(quint64 transportId,
                                           const QSharedPointer<RequestState> &state)
{
    m_requests.remove(transportId);
    m_publicToTransport.remove(state->publicId);
    emit completed(state->publicId);
}

void OpenAiCompatibleProvider::failState(quint64 transportId,
                                         const QSharedPointer<RequestState> &state,
                                         const AiError &error,
                                         bool cancelTransport)
{
    m_requests.remove(transportId);
    m_publicToTransport.remove(state->publicId);
    if (cancelTransport)
        m_transport->cancel(transportId);
    emit failed(state->publicId, error);
}

AiError OpenAiCompatibleProvider::httpError(
        int statusCode, const QMap<QByteArray, QByteArray> &headers,
        const QByteArray &body)
{
    AiError error;
    error.httpStatus = statusCode;
    if (statusCode == 401) {
        error.code = AiErrorCode::AuthenticationFailed;
        error.message = QStringLiteral("The API rejected the configured credential.");
    } else if (statusCode == 403) {
        error.code = AiErrorCode::AuthorizationFailed;
        error.message = QStringLiteral("The API denied access to this operation.");
    } else if (statusCode == 404) {
        error.code = AiErrorCode::NotFound;
        error.message = QStringLiteral("The configured API route or model was not found.");
    } else if (statusCode == 429) {
        error.code = AiErrorCode::RateLimited;
        error.message = QStringLiteral("The API rate limit was reached.");
        error.retryable = true;
        error.retryAfterSeconds = retryAfterSeconds(headers);
    } else if (statusCode >= 400 && statusCode < 500) {
        error.code = AiErrorCode::HttpClientError;
        error.message = QStringLiteral("The API rejected the request.");
    } else {
        error.code = AiErrorCode::HttpServerError;
        error.message = QStringLiteral("The API service returned a server error.");
        error.retryable = true;
    }
    readSafeProviderMetadata(body, &error);
    return error;
}

} // namespace ai
} // namespace smartkey
