#pragma once

#include "aitypes.h"

#include <QObject>

namespace smartkey {
namespace ai {

class AiHttpTransport : public QObject
{
    Q_OBJECT
public:
    explicit AiHttpTransport(QObject *parent = nullptr) : QObject(parent) {}
    ~AiHttpTransport() override = default;

    virtual quint64 start(const AiHttpRequest &request) = 0;
    virtual void cancel(quint64 requestId) = 0;

signals:
    void responseStarted(quint64 requestId, const smartkey::ai::AiHttpResponseHead &head);
    void dataReceived(quint64 requestId, const QByteArray &data);
    void completed(quint64 requestId);
    void failed(quint64 requestId, const smartkey::ai::AiError &error);
};

} // namespace ai
} // namespace smartkey

