#pragma once

#include "aihttptransport.h"

#include <QHash>
#include <QMutex>
#include <memory>

#ifdef Q_OS_WIN
#  include <windows.h>
#  include <winhttp.h>
#endif

namespace smartkey {
namespace ai {

class WinHttpTransport final : public AiHttpTransport
{
    Q_OBJECT
public:
    struct ResponseReadPlan {
        quint32 bytesToRead = 0;
        AiError error;
    };

    explicit WinHttpTransport(QObject *parent = nullptr);
    ~WinHttpTransport() override;

    quint64 start(const AiHttpRequest &request) override;
    void cancel(quint64 requestId) override;

    // Exposed as pure policy helpers so hostile WinHTTP sizes can be tested
    // without allocating memory or requiring a live network request.
    static quint32 responseReadChunkBytes();
    static quint64 responseByteLimit();
    static ResponseReadPlan planResponseRead(quint64 availableBytes,
                                             quint64 receivedBytes);

#ifdef Q_OS_WIN
    // Public only so the C-style WinHTTP callback bridge can hold an opaque
    // forward declaration. These types are implementation-defined.
    struct RequestContext;
    struct CallbackContext;
#endif

private:
#ifdef Q_OS_WIN

    static void CALLBACK statusCallback(HINTERNET handle, DWORD_PTR context,
                                        DWORD status, LPVOID information,
                                        DWORD informationLength);
    static void handleStatus(const std::shared_ptr<RequestContext> &context,
                             DWORD status, LPVOID information,
                             DWORD informationLength);
    static AiError nativeError(DWORD errorCode);
    static AiHttpResponseHead responseHead(HINTERNET requestHandle);
    static bool beginRead(const std::shared_ptr<RequestContext> &context);
    static void closeRequestHandle(const std::shared_ptr<RequestContext> &context);
    static void postFailure(const std::shared_ptr<RequestContext> &context,
                            const AiError &error);
    static void postCompleted(const std::shared_ptr<RequestContext> &context);
    static void postData(const std::shared_ptr<RequestContext> &context,
                         const QByteArray &data);
    static void postHead(const std::shared_ptr<RequestContext> &context,
                         const AiHttpResponseHead &head);
    void forgetRequest(quint64 requestId, const RequestContext *expected);

    HINTERNET m_session = nullptr;
    QHash<quint64, std::shared_ptr<RequestContext>> m_requests;
    QMutex m_requestsMutex;
#endif
    quint64 m_nextRequestId = 1;
};

} // namespace ai
} // namespace smartkey
