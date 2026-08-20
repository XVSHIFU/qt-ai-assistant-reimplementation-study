#include "sseparser.h"

namespace smartkey {
namespace ai {

SseParser::SseParser(qsizetype maximumEventBytes)
    : m_maximumEventBytes(maximumEventBytes)
{
}

void SseParser::reset()
{
    m_buffer.clear();
    m_eventName.clear();
    m_eventId.clear();
    m_eventData.clear();
    m_error = AiError();
    m_firstLine = true;
    m_hasDataField = false;
}

void SseParser::fail(const QString &message)
{
    if (m_error.isError())
        return;
    m_error.code = AiErrorCode::ProtocolError;
    m_error.message = message;
}

QList<SseEvent> SseParser::feed(const QByteArray &chunk)
{
    QList<SseEvent> events;
    if (m_error.isError() || chunk.isEmpty())
        return events;

    if (m_buffer.size() + chunk.size() > m_maximumEventBytes) {
        fail(QStringLiteral("The streaming response exceeded the configured event size limit."));
        return events;
    }

    m_buffer.append(chunk);
    for (;;) {
        const int newline = m_buffer.indexOf('\n');
        if (newline < 0)
            break;
        QByteArray line = m_buffer.left(newline);
        m_buffer.remove(0, newline + 1);
        if (line.endsWith('\r'))
            line.chop(1);
        consumeLine(line, &events);
        if (m_error.isError())
            break;
    }
    return events;
}

QList<SseEvent> SseParser::finish()
{
    QList<SseEvent> events;
    if (m_error.isError())
        return events;
    if (!m_buffer.isEmpty()) {
        QByteArray line = m_buffer;
        m_buffer.clear();
        if (line.endsWith('\r'))
            line.chop(1);
        consumeLine(line, &events);
    }
    dispatch(&events);
    return events;
}

void SseParser::consumeLine(QByteArray line, QList<SseEvent> *events)
{
    if (m_firstLine) {
        m_firstLine = false;
        if (line.startsWith("\xEF\xBB\xBF"))
            line.remove(0, 3);
    }

    if (line.isEmpty()) {
        dispatch(events);
        return;
    }
    if (line.startsWith(':'))
        return;

    const int colon = line.indexOf(':');
    QByteArray field = colon < 0 ? line : line.left(colon);
    QByteArray value = colon < 0 ? QByteArray() : line.mid(colon + 1);
    if (value.startsWith(' '))
        value.remove(0, 1);

    if (field == "data") {
        if (m_hasDataField)
            m_eventData.append('\n');
        m_eventData.append(value);
        m_hasDataField = true;
    } else if (field == "event") {
        m_eventName = value;
    } else if (field == "id" && !value.contains('\0')) {
        m_eventId = value;
    }

    if (m_eventData.size() + m_eventName.size() + m_eventId.size() > m_maximumEventBytes)
        fail(QStringLiteral("The streaming response exceeded the configured event size limit."));
}

void SseParser::dispatch(QList<SseEvent> *events)
{
    if (!m_hasDataField) {
        m_eventName.clear();
        return;
    }
    SseEvent event;
    event.event = m_eventName;
    event.id = m_eventId;
    event.data = m_eventData;
    events->append(event);
    m_eventName.clear();
    m_eventData.clear();
    m_hasDataField = false;
}

} // namespace ai
} // namespace smartkey
