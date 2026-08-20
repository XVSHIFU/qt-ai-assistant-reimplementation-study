#include "rapooaikeyadapter.h"

#include <QMetaMethod>

RapooAiKeyAdapter::RapooAiKeyAdapter(QObject *parent)
    : QObject(parent),
      m_status(tr("未连接兼容的雷柏 AI 键设备接口；通用全局快捷键仍可使用。"))
{
}

bool RapooAiKeyAdapter::attachDeviceSource(QObject *source)
{
    detachDeviceSource();
    if (!source) {
        const QString reason = tr("雷柏设备接口不可用；已降级为通用全局快捷键。");
        setAvailability(false, reason);
        emit unavailable(reason);
        return false;
    }

    const QMetaObject *sourceMetaObject = source->metaObject();
    int sourceSignalIndex = sourceMetaObject->indexOfSignal("reviceDeviceAIKeyPresssed()");
    if (sourceSignalIndex < 0)
        sourceSignalIndex = sourceMetaObject->indexOfSignal("deviceAIKeyPressed()");
    const int targetSignalIndex = metaObject()->indexOfSignal("activated()");
    if (sourceSignalIndex < 0 || targetSignalIndex < 0) {
        const QString reason = tr("雷柏设备接口未公开兼容的 AI 键信号；已降级为通用全局快捷键。");
        setAvailability(false, reason);
        emit unavailable(reason);
        return false;
    }

    m_source = source;
    m_deviceConnection = QObject::connect(source,
                                          sourceMetaObject->method(sourceSignalIndex),
                                          this,
                                          metaObject()->method(targetSignalIndex),
                                          Qt::UniqueConnection);
    m_destroyedConnection = connect(source, &QObject::destroyed, this, [this] {
        m_source.clear();
        setAvailability(false, tr("雷柏设备接口已断开；通用全局快捷键仍可使用。"));
    });
    setAvailability(true, tr("雷柏 AI 键接口已连接。"));
    return true;
}

void RapooAiKeyAdapter::detachDeviceSource()
{
    if (m_deviceConnection)
        disconnect(m_deviceConnection);
    if (m_destroyedConnection)
        disconnect(m_destroyedConnection);
    m_deviceConnection = QMetaObject::Connection();
    m_destroyedConnection = QMetaObject::Connection();
    m_source.clear();
    setAvailability(false, tr("未连接兼容的雷柏 AI 键设备接口；通用全局快捷键仍可使用。"));
}

bool RapooAiKeyAdapter::start()
{
    if (m_available)
        return true;
    emit unavailable(m_status);
    return false;
}

void RapooAiKeyAdapter::stop()
{
    detachDeviceSource();
}

void RapooAiKeyAdapter::setAvailability(bool available, const QString &status)
{
    if (m_available != available) {
        m_available = available;
        emit availabilityChanged(m_available);
    }
    if (m_status != status) {
        m_status = status;
        emit statusChanged(m_status);
    }
}
