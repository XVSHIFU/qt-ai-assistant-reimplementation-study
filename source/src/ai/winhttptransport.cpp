#include "winhttptransport.h"

#include "urlpolicy.h"

#include <QMetaObject>
#include <QMutexLocker>
#include <QStringList>
#include <QTimer>

#include <limits>
#include <string>
#include <vector>

namespace smartkey {
namespace ai {

namespace {

constexpr quint32 kResponseReadChunkBytes = 64u * 1024u;
constexpr quint64 kResponseByteLimit = quint64(64u) * 1024u * 1024u;
static_assert(kResponseReadChunkBytes <= quint32(std::numeric_limits<int>::max()),
              "WinHTTP read chunks must fit in QByteArray's int size");

AiError responseLimitError()
{
    AiError error;
    error.code = AiErrorCode::ProtocolError;
    error.message = QStringLiteral("The API response exceeded the 64 MiB transport limit.");
    return error;
}

} // namespace

#ifdef Q_OS_WIN

struct WinHttpTransport::RequestContext {
    QMutex mutex;
    WinHttpTransport *owner = nullptr;
    quint64 id = 0;
    HINTERNET connection = nullptr;
    HINTERNET request = nullptr;
    QByteArray body;
    QByteArray readBuffer;
    std::wstring headers;
    quint64 responseBytes = 0;
    bool terminal = false;
};

struct WinHttpTransport::CallbackContext {
    std::shared_ptr<RequestContext> request;
};

namespace {

bool isPendingOrSuccessful(BOOL result)
{
    return result || GetLastError() == ERROR_IO_PENDING;
}

QString responseHeader(HINTERNET request, DWORD query, const wchar_t *customName = nullptr)
{
    DWORD bytes = 0;
    LPCWSTR name = customName;
    WinHttpQueryHeaders(request, query, name, WINHTTP_NO_OUTPUT_BUFFER, &bytes, WINHTTP_NO_HEADER_INDEX);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || bytes < sizeof(wchar_t))
        return QString();

    std::vector<wchar_t> buffer(bytes / sizeof(wchar_t) + 1, L'\0');
    if (!WinHttpQueryHeaders(request, query, name, buffer.data(), &bytes, WINHTTP_NO_HEADER_INDEX))
        return QString();
    return QString::fromWCharArray(buffer.data()).trimmed();
}

template <typename Function>
void postToOwner(const std::shared_ptr<WinHttpTransport::RequestContext> &context,
                 Function function)
{
    WinHttpTransport *owner = nullptr;
    {
        QMutexLocker locker(&context->mutex);
        owner = context->owner;
    }
    if (owner)
        QMetaObject::invokeMethod(owner, function, Qt::QueuedConnection);
}

bool markTerminal(const std::shared_ptr<WinHttpTransport::RequestContext> &context)
{
    QMutexLocker locker(&context->mutex);
    if (context->terminal)
        return false;
    context->terminal = true;
    return true;
}

} // namespace

#endif

quint32 WinHttpTransport::responseReadChunkBytes()
{
    return kResponseReadChunkBytes;
}

quint64 WinHttpTransport::responseByteLimit()
{
    return kResponseByteLimit;
}

WinHttpTransport::ResponseReadPlan WinHttpTransport::planResponseRead(
        quint64 availableBytes,
        quint64 receivedBytes)
{
    ResponseReadPlan plan;
    if (availableBytes == 0)
        return plan;

    if (receivedBytes > kResponseByteLimit
            || availableBytes > kResponseByteLimit - receivedBytes) {
        plan.error = responseLimitError();
        return plan;
    }

    plan.bytesToRead = static_cast<quint32>(
            qMin(availableBytes, quint64(kResponseReadChunkBytes)));
    return plan;
}

WinHttpTransport::WinHttpTransport(QObject *parent)
    : AiHttpTransport(parent)
{
    qRegisterMetaType<AiError>();
    qRegisterMetaType<AiHttpResponseHead>();
#ifdef Q_OS_WIN
    m_session = WinHttpOpen(L"SmartKeyAI/2.0",
                            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                            WINHTTP_NO_PROXY_NAME,
                            WINHTTP_NO_PROXY_BYPASS,
                            WINHTTP_FLAG_ASYNC);
#endif
}

WinHttpTransport::~WinHttpTransport()
{
#ifdef Q_OS_WIN
    QList<std::shared_ptr<RequestContext>> requests;
    {
        QMutexLocker locker(&m_requestsMutex);
        requests = m_requests.values();
        m_requests.clear();
    }
    for (const std::shared_ptr<RequestContext> &context : requests) {
        {
            QMutexLocker locker(&context->mutex);
            context->owner = nullptr;
            context->terminal = true;
        }
        closeRequestHandle(context);
    }
    if (m_session)
        WinHttpCloseHandle(m_session);
#endif
}

quint64 WinHttpTransport::start(const AiHttpRequest &input)
{
    const UrlPolicyResult policy = UrlPolicy::validateEndpoint(input.url, input.allowLoopbackHttp);
    if (!policy.accepted) {
        QTimer::singleShot(0, this, [this, policy] { emit failed(0, policy.error); });
        return 0;
    }

#ifndef Q_OS_WIN
    AiError error;
    error.code = AiErrorCode::Unsupported;
    error.message = QStringLiteral("WinHTTP transport is available only on Windows.");
    QTimer::singleShot(0, this, [this, error] { emit failed(0, error); });
    return 0;
#else
    if (!m_session) {
        const AiError error = nativeError(GetLastError());
        QTimer::singleShot(0, this, [this, error] { emit failed(0, error); });
        return 0;
    }
    if (quint64(input.body.size()) > quint64(std::numeric_limits<DWORD>::max())) {
        AiError error;
        error.code = AiErrorCode::InvalidRequest;
        error.message = QStringLiteral("The request body is too large.");
        QTimer::singleShot(0, this, [this, error] { emit failed(0, error); });
        return 0;
    }

    const QUrl url = policy.canonicalUrl;
    const std::wstring host = url.host().toStdWString();
    const INTERNET_PORT port = INTERNET_PORT(url.port(url.scheme() == QStringLiteral("https")
                                                       ? INTERNET_DEFAULT_HTTPS_PORT
                                                       : INTERNET_DEFAULT_HTTP_PORT));
    HINTERNET connection = WinHttpConnect(m_session, host.c_str(), port, 0);
    if (!connection) {
        const AiError error = nativeError(GetLastError());
        QTimer::singleShot(0, this, [this, error] { emit failed(0, error); });
        return 0;
    }

    QString resource = url.path(QUrl::FullyEncoded);
    if (resource.isEmpty())
        resource = QStringLiteral("/");
    if (url.hasQuery())
        resource += QStringLiteral("?") + url.query(QUrl::FullyEncoded);
    const std::wstring resourceWide = resource.toStdWString();
    const std::wstring methodWide = QString::fromLatin1(input.method).toUpper().toStdWString();
    const DWORD flags = url.scheme() == QStringLiteral("https") ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET requestHandle = WinHttpOpenRequest(connection,
                                                 methodWide.c_str(),
                                                 resourceWide.c_str(),
                                                 nullptr,
                                                 WINHTTP_NO_REFERER,
                                                 WINHTTP_DEFAULT_ACCEPT_TYPES,
                                                 flags);
    if (!requestHandle) {
        const AiError error = nativeError(GetLastError());
        WinHttpCloseHandle(connection);
        QTimer::singleShot(0, this, [this, error] { emit failed(0, error); });
        return 0;
    }

    std::shared_ptr<RequestContext> context = std::make_shared<RequestContext>();
    context->owner = this;
    context->id = m_nextRequestId++;
    context->connection = connection;
    context->request = requestHandle;
    context->body = input.body;

    QStringList headerLines;
    for (auto it = input.headers.constBegin(); it != input.headers.constEnd(); ++it)
        headerLines.append(QString::fromLatin1(it.key()) + QStringLiteral(": ") + QString::fromLatin1(it.value()));
    context->headers = headerLines.join(QStringLiteral("\r\n")).toStdWString();

    DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    WinHttpSetOption(requestHandle, WINHTTP_OPTION_REDIRECT_POLICY,
                     &redirectPolicy, sizeof(redirectPolicy));
    const int timeout = qBound(1000, input.timeoutMs, 10 * 60 * 1000);
    WinHttpSetTimeouts(requestHandle, timeout, timeout, timeout, timeout);

    CallbackContext *callbackContext = new CallbackContext{context};
    DWORD_PTR callbackValue = reinterpret_cast<DWORD_PTR>(callbackContext);
    WinHttpSetOption(requestHandle, WINHTTP_OPTION_CONTEXT_VALUE,
                     &callbackValue, sizeof(callbackValue));
    const DWORD callbackFlags = WINHTTP_CALLBACK_FLAG_ALL_COMPLETIONS
            | WINHTTP_CALLBACK_FLAG_SECURE_FAILURE
            | WINHTTP_CALLBACK_FLAG_REDIRECT
            | WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING;
    if (WinHttpSetStatusCallback(requestHandle, &WinHttpTransport::statusCallback,
                                 callbackFlags, 0) == WINHTTP_INVALID_STATUS_CALLBACK) {
        const AiError error = nativeError(GetLastError());
        delete callbackContext;
        WinHttpCloseHandle(requestHandle);
        WinHttpCloseHandle(connection);
        QTimer::singleShot(0, this, [this, error] { emit failed(0, error); });
        return 0;
    }

    {
        QMutexLocker locker(&m_requestsMutex);
        m_requests.insert(context->id, context);
    }

    const wchar_t *headers = context->headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS
                                                       : context->headers.c_str();
    const DWORD headersLength = context->headers.empty() ? 0 : DWORD(context->headers.size());
    LPVOID body = context->body.isEmpty() ? WINHTTP_NO_REQUEST_DATA : context->body.data();
    const DWORD bodyLength = DWORD(context->body.size());
    if (!isPendingOrSuccessful(WinHttpSendRequest(requestHandle,
                                                   headers,
                                                   headersLength,
                                                   body,
                                                   bodyLength,
                                                   bodyLength,
                                                   callbackValue))) {
        postFailure(context, nativeError(GetLastError()));
    }
    return context->id;
#endif
}

void WinHttpTransport::cancel(quint64 requestId)
{
#ifdef Q_OS_WIN
    std::shared_ptr<RequestContext> context;
    {
        QMutexLocker locker(&m_requestsMutex);
        context = m_requests.value(requestId);
    }
    if (!context || !markTerminal(context))
        return;
    AiError error;
    error.code = AiErrorCode::Cancelled;
    error.message = QStringLiteral("The request was cancelled.");
    emit failed(requestId, error);
    closeRequestHandle(context);
#else
    Q_UNUSED(requestId)
#endif
}

#ifdef Q_OS_WIN

void CALLBACK WinHttpTransport::statusCallback(HINTERNET, DWORD_PTR opaque,
                                                DWORD status, LPVOID information,
                                                DWORD informationLength)
{
    CallbackContext *callback = reinterpret_cast<CallbackContext *>(opaque);
    if (!callback)
        return;
    const std::shared_ptr<RequestContext> context = callback->request;
    handleStatus(context, status, information, informationLength);
    if (status == WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING)
        delete callback;
}

void WinHttpTransport::handleStatus(const std::shared_ptr<RequestContext> &context,
                                    DWORD status, LPVOID information,
                                    DWORD informationLength)
{
    if (status == WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING) {
        HINTERNET connection = nullptr;
        WinHttpTransport *owner = nullptr;
        {
            QMutexLocker locker(&context->mutex);
            context->request = nullptr;
            connection = context->connection;
            context->connection = nullptr;
            owner = context->owner;
        }
        if (connection)
            WinHttpCloseHandle(connection);
        if (owner) {
            const quint64 id = context->id;
            const RequestContext *expected = context.get();
            QMetaObject::invokeMethod(owner, [owner, id, expected] {
                owner->forgetRequest(id, expected);
            }, Qt::QueuedConnection);
        }
        return;
    }

    {
        QMutexLocker locker(&context->mutex);
        if (context->terminal)
            return;
    }

    switch (status) {
    case WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE:
        if (!isPendingOrSuccessful(WinHttpReceiveResponse(context->request, nullptr)))
            postFailure(context, nativeError(GetLastError()));
        break;
    case WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE: {
        const AiHttpResponseHead head = responseHead(context->request);
        postHead(context, head);
        if (head.statusCode >= 300 && head.statusCode < 400) {
            AiError error;
            error.code = AiErrorCode::RedirectBlocked;
            error.httpStatus = head.statusCode;
            error.message = QStringLiteral("The API endpoint attempted a redirect. Update the configured endpoint explicitly.");
            postFailure(context, error);
        } else {
            beginRead(context);
        }
        break;
    }
    case WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE: {
        const DWORD available = information && informationLength >= sizeof(DWORD)
                ? *static_cast<DWORD *>(information) : 0;
        if (available == 0) {
            postCompleted(context);
            break;
        }

        quint64 responseBytes = 0;
        {
            QMutexLocker locker(&context->mutex);
            responseBytes = context->responseBytes;
        }
        const ResponseReadPlan plan = planResponseRead(quint64(available), responseBytes);
        if (plan.error.isError()) {
            postFailure(context, plan.error);
            break;
        }

        const int bufferSize = static_cast<int>(plan.bytesToRead);
        {
            QMutexLocker locker(&context->mutex);
            context->readBuffer.resize(bufferSize);
        }
        LPVOID destination = nullptr;
        {
            QMutexLocker locker(&context->mutex);
            destination = context->readBuffer.data();
        }
        if (!isPendingOrSuccessful(WinHttpReadData(context->request,
                                                    destination,
                                                    DWORD(plan.bytesToRead),
                                                    nullptr)))
            postFailure(context, nativeError(GetLastError()));
        break;
    }
    case WINHTTP_CALLBACK_STATUS_READ_COMPLETE: {
        bool invalidReadLength = false;
        {
            QMutexLocker locker(&context->mutex);
            const quint64 completedBytes = quint64(informationLength);
            invalidReadLength = informationLength > DWORD(context->readBuffer.size())
                    || context->responseBytes > kResponseByteLimit
                    || completedBytes > kResponseByteLimit - context->responseBytes;
            if (!invalidReadLength)
                context->responseBytes += completedBytes;
        }
        if (invalidReadLength) {
            postFailure(context, responseLimitError());
            break;
        }
        if (informationLength > 0) {
            // Safe after validating against the fixed-size QByteArray above.
            postData(context, QByteArray(static_cast<const char *>(information),
                                         static_cast<int>(informationLength)));
        }
        beginRead(context);
        break;
    }
    case WINHTTP_CALLBACK_STATUS_REQUEST_ERROR: {
        const WINHTTP_ASYNC_RESULT *result = static_cast<const WINHTTP_ASYNC_RESULT *>(information);
        postFailure(context, nativeError(result ? result->dwError : ERROR_WINHTTP_INTERNAL_ERROR));
        break;
    }
    case WINHTTP_CALLBACK_STATUS_SECURE_FAILURE: {
        AiError error;
        error.code = AiErrorCode::TlsFailure;
        error.nativeCode = information && informationLength >= sizeof(DWORD)
                ? *static_cast<DWORD *>(information) : ERROR_WINHTTP_SECURE_FAILURE;
        error.message = QStringLiteral("TLS certificate or secure-channel validation failed.");
        postFailure(context, error);
        break;
    }
    case WINHTTP_CALLBACK_STATUS_REDIRECT: {
        AiError error;
        error.code = AiErrorCode::RedirectBlocked;
        error.message = QStringLiteral("Automatic redirects are disabled for API requests.");
        postFailure(context, error);
        break;
    }
    default:
        break;
    }
}

bool WinHttpTransport::beginRead(const std::shared_ptr<RequestContext> &context)
{
    if (!isPendingOrSuccessful(WinHttpQueryDataAvailable(context->request, nullptr))) {
        postFailure(context, nativeError(GetLastError()));
        return false;
    }
    return true;
}

void WinHttpTransport::postFailure(const std::shared_ptr<RequestContext> &context,
                                   const AiError &error)
{
    if (!markTerminal(context))
        return;
    const quint64 id = context->id;
    postToOwner(context, [context, id, error] {
        WinHttpTransport *owner = nullptr;
        {
            QMutexLocker locker(&context->mutex);
            owner = context->owner;
        }
        if (owner)
            emit owner->failed(id, error);
        closeRequestHandle(context);
    });
}

void WinHttpTransport::postCompleted(const std::shared_ptr<RequestContext> &context)
{
    if (!markTerminal(context))
        return;
    const quint64 id = context->id;
    postToOwner(context, [context, id] {
        WinHttpTransport *owner = nullptr;
        {
            QMutexLocker locker(&context->mutex);
            owner = context->owner;
        }
        if (owner)
            emit owner->completed(id);
        closeRequestHandle(context);
    });
}

void WinHttpTransport::postData(const std::shared_ptr<RequestContext> &context,
                                const QByteArray &data)
{
    const quint64 id = context->id;
    postToOwner(context, [context, id, data] {
        WinHttpTransport *owner = nullptr;
        {
            QMutexLocker locker(&context->mutex);
            owner = context->owner;
        }
        if (owner)
            emit owner->dataReceived(id, data);
    });
}

void WinHttpTransport::postHead(const std::shared_ptr<RequestContext> &context,
                                const AiHttpResponseHead &head)
{
    const quint64 id = context->id;
    postToOwner(context, [context, id, head] {
        WinHttpTransport *owner = nullptr;
        {
            QMutexLocker locker(&context->mutex);
            owner = context->owner;
        }
        if (owner)
            emit owner->responseStarted(id, head);
    });
}

void WinHttpTransport::closeRequestHandle(const std::shared_ptr<RequestContext> &context)
{
    HINTERNET request = nullptr;
    {
        QMutexLocker locker(&context->mutex);
        request = context->request;
        context->request = nullptr;
    }
    if (request)
        WinHttpCloseHandle(request);
}

AiHttpResponseHead WinHttpTransport::responseHead(HINTERNET requestHandle)
{
    AiHttpResponseHead head;
    DWORD status = 0;
    DWORD statusBytes = sizeof(status);
    WinHttpQueryHeaders(requestHandle,
                        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX,
                        &status, &statusBytes, WINHTTP_NO_HEADER_INDEX);
    head.statusCode = int(status);
    const QString contentType = responseHeader(requestHandle, WINHTTP_QUERY_CONTENT_TYPE);
    if (!contentType.isEmpty())
        head.headers.insert(QByteArrayLiteral("content-type"), contentType.toLatin1());
    const QString location = responseHeader(requestHandle, WINHTTP_QUERY_LOCATION);
    if (!location.isEmpty())
        head.headers.insert(QByteArrayLiteral("location"), location.toUtf8());
    const QString retryAfter = responseHeader(requestHandle, WINHTTP_QUERY_CUSTOM, L"Retry-After");
    if (!retryAfter.isEmpty())
        head.headers.insert(QByteArrayLiteral("retry-after"), retryAfter.toLatin1());
    return head;
}

AiError WinHttpTransport::nativeError(DWORD errorCode)
{
    AiError error;
    error.nativeCode = errorCode;
    switch (errorCode) {
    case 12163u: // ERROR_INTERNET_DISCONNECTED (shared WinINet system code)
        error.code = AiErrorCode::Offline;
        error.message = QStringLiteral("The computer appears to be offline.");
        error.retryable = true;
        break;
    case ERROR_WINHTTP_NAME_NOT_RESOLVED:
        error.code = AiErrorCode::DnsFailure;
        error.message = QStringLiteral("The API host name could not be resolved.");
        error.retryable = true;
        break;
    case ERROR_WINHTTP_CANNOT_CONNECT:
    case ERROR_WINHTTP_CONNECTION_ERROR:
        error.code = AiErrorCode::ConnectionFailure;
        error.message = QStringLiteral("A connection to the API endpoint could not be established.");
        error.retryable = true;
        break;
    case ERROR_WINHTTP_SECURE_FAILURE:
    case ERROR_WINHTTP_CLIENT_AUTH_CERT_NEEDED:
        error.code = AiErrorCode::TlsFailure;
        error.message = QStringLiteral("TLS certificate or secure-channel validation failed.");
        break;
    case ERROR_WINHTTP_TIMEOUT:
        error.code = AiErrorCode::Timeout;
        error.message = QStringLiteral("The API request timed out.");
        error.retryable = true;
        break;
    case ERROR_WINHTTP_OPERATION_CANCELLED:
        error.code = AiErrorCode::Cancelled;
        error.message = QStringLiteral("The request was cancelled.");
        break;
    case ERROR_WINHTTP_AUTODETECTION_FAILED:
    case ERROR_WINHTTP_BAD_AUTO_PROXY_SCRIPT:
    case ERROR_WINHTTP_UNABLE_TO_DOWNLOAD_SCRIPT:
        error.code = AiErrorCode::ProxyFailure;
        error.message = QStringLiteral("Windows proxy discovery failed.");
        error.retryable = true;
        break;
    default:
        error.code = AiErrorCode::NetworkFailure;
        error.message = QStringLiteral("The API network request failed.");
        error.retryable = true;
        break;
    }
    return error;
}

void WinHttpTransport::forgetRequest(quint64 requestId, const RequestContext *expected)
{
    QMutexLocker locker(&m_requestsMutex);
    const auto it = m_requests.find(requestId);
    if (it != m_requests.end() && it.value().get() == expected)
        m_requests.erase(it);
}

#endif

} // namespace ai
} // namespace smartkey
