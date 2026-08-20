#pragma once

#include <QByteArray>
#include <QList>
#include <QMap>
#include <QMetaType>
#include <QString>
#include <QUrl>
#include <QVariantMap>

namespace smartkey {
namespace ai {

enum class AiEventKind {
    ContentDelta,
    ReasoningDelta,
    ReferenceDelta,
    Completed
};

struct AiStreamEvent {
    AiEventKind kind = AiEventKind::ContentDelta;
    QString text;
};

enum class AiErrorCode {
    None,
    InvalidUrl,
    UnsupportedScheme,
    InsecureUrl,
    InvalidRequest,
    Busy,
    Offline,
    DnsFailure,
    ConnectionFailure,
    TlsFailure,
    ProxyFailure,
    Timeout,
    Cancelled,
    RedirectBlocked,
    AuthenticationFailed,
    AuthorizationFailed,
    NotFound,
    RateLimited,
    HttpClientError,
    HttpServerError,
    NetworkFailure,
    ProtocolError,
    ParseError,
    Unsupported,
    LegacyDisabled
};

struct AiError {
    AiErrorCode code = AiErrorCode::None;
    QString message;
    int httpStatus = 0;
    quint32 nativeCode = 0;
    bool retryable = false;
    int retryAfterSeconds = -1;
    // Provider metadata is retained only when it matches the conservative
    // token policy enforced by the provider implementation. Response bodies,
    // endpoint URLs and credentials never belong in this value type.
    QString providerCode;
    QString providerType;

    bool isError() const { return code != AiErrorCode::None; }
};

struct AiMessage {
    QString role;
    QString content;
};

struct AiChatRequest {
    QList<AiMessage> messages;
    QString model;
    bool stream = true;
    bool reasoningEnabled = false;
    bool searchEnabled = false;
    QVariantMap additionalBody;
};

using AiRequest = AiChatRequest;

enum class AiAuthMode {
    None,
    Bearer,
    Header
};

// apiKey is deliberately a transient runtime value. Persist only a credential
// reference outside this type (for example, in Windows Credential Manager).
struct OpenAiChatConfig {
    QUrl endpoint;
    QString defaultModel;
    AiAuthMode authMode = AiAuthMode::Bearer;
    QByteArray apiKey;
    QByteArray apiKeyHeader = QByteArrayLiteral("api-key");
    QMap<QByteArray, QByteArray> additionalHeaders;
    QVariantMap additionalBody;
    QByteArray reasoningRequestField;
    QByteArray searchRequestField;
    int timeoutMs = 60000;
    bool allowLoopbackHttp = false;
    // Some OpenAI-compatible servers intentionally close an SSE response
    // without [DONE] or finish_reason. This compatibility escape hatch must
    // be enabled explicitly; strict terminal-event validation is the default.
    bool allowSseEofCompletion = false;
};

using AiEndpointConfig = OpenAiChatConfig;

struct AiHttpRequest {
    QUrl url;
    QByteArray method = QByteArrayLiteral("POST");
    QMap<QByteArray, QByteArray> headers;
    QByteArray body;
    int timeoutMs = 60000;
    bool allowLoopbackHttp = false;
};

struct AiHttpResponseHead {
    int statusCode = 0;
    QMap<QByteArray, QByteArray> headers;
};

} // namespace ai
} // namespace smartkey

Q_DECLARE_METATYPE(smartkey::ai::AiStreamEvent)
Q_DECLARE_METATYPE(smartkey::ai::AiErrorCode)
Q_DECLARE_METATYPE(smartkey::ai::AiError)
Q_DECLARE_METATYPE(smartkey::ai::AiHttpResponseHead)
