#pragma once

#include "aihttptransport.h"

#include <QHash>
#include <QScopedPointer>

namespace smartkey {
namespace ai {

class ModelDiscoveryService final : public QObject
{
    Q_OBJECT
public:
    explicit ModelDiscoveryService(QObject *parent = nullptr);
    explicit ModelDiscoveryService(AiHttpTransport *transport, QObject *parent = nullptr);

    static AiError buildRequest(const QVariantMap &profile, const QByteArray &credential,
                                AiHttpRequest *request);
    void discover(const QString &profileId, const QVariantMap &profile,
                  const QByteArray &credential);
    void cancelProfile(const QString &profileId);
    void cancelAll();

signals:
    void discovered(const QString &profileId, const QStringList &models);
    void failed(const QString &profileId, const smartkey::ai::AiError &error);

private slots:
    void onResponseStarted(quint64 transportId, const smartkey::ai::AiHttpResponseHead &head);
    void onDataReceived(quint64 transportId, const QByteArray &data);
    void onCompleted(quint64 transportId);
    void onFailed(quint64 transportId, const smartkey::ai::AiError &error);

private:
    struct RequestState {
        QString profileId;
        quint64 generation = 0;
        int statusCode = 0;
        QByteArray body;
    };

    void initializeTransport();
    quint64 advanceGeneration(const QString &profileId);
    bool isCurrent(quint64 transportId, const RequestState &state) const;
    void removeRequest(quint64 transportId, const RequestState &state);
    static AiError httpError(int statusCode);
    static AiError parseModels(const QByteArray &body, QStringList *models);

    QScopedPointer<AiHttpTransport> m_ownedTransport;
    AiHttpTransport *m_transport = nullptr;
    QHash<quint64, RequestState> m_requests;
    QHash<QString, quint64> m_activeRequests;
    QHash<QString, quint64> m_latestGenerations;
    quint64 m_nextGeneration = 1;
};

} // namespace ai
} // namespace smartkey
