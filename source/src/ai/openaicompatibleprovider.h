#pragma once

#include "aihttptransport.h"
#include "aitypes.h"
#include "sseparser.h"

#include <QHash>
#include <QJsonValue>
#include <QObject>
#include <QSharedPointer>

namespace smartkey {
namespace ai {

class OpenAiCompatibleProvider final : public QObject
{
    Q_OBJECT
public:
    explicit OpenAiCompatibleProvider(AiHttpTransport *transport = nullptr,
                                      QObject *parent = nullptr);
    ~OpenAiCompatibleProvider() override;

    bool configure(const AiEndpointConfig &config, AiError *error = nullptr);

    QString start(const AiRequest &request);
    void cancel(const QString &requestId);
    void cancelAll();

    static AiError validateConfig(const AiEndpointConfig &config);
    static AiError buildHttpRequest(const AiEndpointConfig &config,
                                    const AiRequest &request,
                                    AiHttpRequest *httpRequest);

signals:
    void contentDelta(const QString &requestId, const QString &text);
    void reasoningDelta(const QString &requestId, const QString &text);
    void referenceDelta(const QString &requestId, const QString &text);
    void usage(const QString &requestId, const QVariantMap &usageData);
    void completed(const QString &requestId);
    void failed(const QString &requestId, const smartkey::ai::AiError &error);

private:
    struct RequestState {
        RequestState() : parser(1024 * 1024) {}
        QString publicId;
        AiRequest request;
        SseParser parser;
        QByteArray body;
        QMap<QByteArray, QByteArray> responseHeaders;
        int statusCode = 0;
        bool usesSse = false;
        bool sawSseEvent = false;
        bool sawDone = false;
        bool sawFinishReason = false;
        bool allowSseEofCompletion = false;
    };

    void onResponseStarted(quint64 transportId, const AiHttpResponseHead &head);
    void onDataReceived(quint64 transportId, const QByteArray &data);
    void onTransportCompleted(quint64 transportId);
    void onTransportFailed(quint64 transportId, const AiError &error);
    bool consumeSseEvents(quint64 transportId,
                          const QSharedPointer<RequestState> &state,
                          const QList<SseEvent> &events);
    bool consumeJsonPayload(quint64 transportId,
                            const QSharedPointer<RequestState> &state,
                            const QByteArray &payload,
                            bool streaming);
    void finishState(quint64 transportId, const QSharedPointer<RequestState> &state);
    void failState(quint64 transportId, const QSharedPointer<RequestState> &state,
                   const AiError &error, bool cancelTransport);
    static AiError httpError(int statusCode,
                             const QMap<QByteArray, QByteArray> &headers,
                             const QByteArray &body);
    static QString jsonText(const QJsonValue &value);
    static void secureClear(QByteArray *value);

    AiHttpTransport *m_transport = nullptr;
    AiEndpointConfig m_config;
    QHash<quint64, QSharedPointer<RequestState>> m_requests;
    QHash<QString, quint64> m_publicToTransport;
};

} // namespace ai
} // namespace smartkey
