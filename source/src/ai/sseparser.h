#pragma once

#include "aitypes.h"

#include <QByteArray>
#include <QList>

namespace smartkey {
namespace ai {

struct SseEvent {
    QByteArray event;
    QByteArray id;
    QByteArray data;
};

class SseParser final
{
public:
    explicit SseParser(qsizetype maximumEventBytes = 1024 * 1024);

    QList<SseEvent> feed(const QByteArray &chunk);
    QList<SseEvent> finish();
    void reset();

    bool hasError() const { return m_error.isError(); }
    AiError error() const { return m_error; }

private:
    void consumeLine(QByteArray line, QList<SseEvent> *events);
    void dispatch(QList<SseEvent> *events);
    void fail(const QString &message);

    QByteArray m_buffer;
    QByteArray m_eventName;
    QByteArray m_eventId;
    QByteArray m_eventData;
    qsizetype m_maximumEventBytes;
    AiError m_error;
    bool m_firstLine = true;
    bool m_hasDataField = false;
};

} // namespace ai
} // namespace smartkey
