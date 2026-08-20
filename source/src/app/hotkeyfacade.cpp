#include "hotkeyfacade.h"

#include "platform/hotkeyservice.h"

#include <QKeySequence>
#include <QSettings>

HotkeyFacade::HotkeyFacade(HotkeyService *service, QObject *parent)
    : QObject(parent), m_service(service)
{
    if (!m_service)
        return;
    connect(m_service, &HotkeyService::activated, this, &HotkeyFacade::activated);
    connect(m_service, &HotkeyService::enabledChanged, this, &HotkeyFacade::registeredChanged);
    connect(m_service, &HotkeyService::bindingChanged, this, [this] {
        emit sequenceChanged();
        emit registeredChanged();
    });
    connect(m_service, &HotkeyService::registrationFailed, this,
            [this](const QString &message) { setError(message, true); });
}

QString HotkeyFacade::sequence() const
{
    if (!m_service)
        return QStringLiteral("Ctrl+Alt+Space");
    int value = 0;
    const quint32 modifiers = m_service->modifiers();
    if (modifiers & HotkeyService::Control) value |= Qt::CTRL;
    if (modifiers & HotkeyService::Alt) value |= Qt::ALT;
    if (modifiers & HotkeyService::Shift) value |= Qt::SHIFT;
    if (modifiers & HotkeyService::Win) value |= Qt::META;
    const quint32 vk = m_service->virtualKey();
    if (vk >= 0x70 && vk <= 0x87)
        value |= Qt::Key_F1 + static_cast<int>(vk - 0x70);
    else
        value |= static_cast<int>(vk);
    return QKeySequence(value).toString(QKeySequence::NativeText);
}

bool HotkeyFacade::enabled() const { return m_service && m_service->isEnabled(); }
bool HotkeyFacade::registered() const { return enabled(); }

bool HotkeyFacade::parseSequence(const QString &text, quint32 *modifiers,
                                 quint32 *virtualKey, QString *errorMessage)
{
    const QKeySequence sequence = QKeySequence::fromString(text, QKeySequence::NativeText);
    if (sequence.isEmpty()) {
        if (errorMessage) *errorMessage = QObject::tr("快捷键格式无效。");
        return false;
    }
    const int combined = sequence[0];
    quint32 nativeModifiers = 0;
    if (combined & Qt::CTRL) nativeModifiers |= HotkeyService::Control;
    if (combined & Qt::ALT) nativeModifiers |= HotkeyService::Alt;
    if (combined & Qt::SHIFT) nativeModifiers |= HotkeyService::Shift;
    if (combined & Qt::META) nativeModifiers |= HotkeyService::Win;
    const int key = combined & ~Qt::KeyboardModifierMask;
    quint32 vk = 0;
    if ((key >= Qt::Key_A && key <= Qt::Key_Z)
            || (key >= Qt::Key_0 && key <= Qt::Key_9)
            || key == Qt::Key_Space) {
        vk = static_cast<quint32>(key);
    } else if (key >= Qt::Key_F1 && key <= Qt::Key_F24) {
        vk = 0x70u + static_cast<quint32>(key - Qt::Key_F1);
    } else {
        if (errorMessage) *errorMessage = QObject::tr("仅支持字母、数字、空格或 F1–F24。");
        return false;
    }
    if (modifiers) *modifiers = nativeModifiers;
    if (virtualKey) *virtualKey = vk;
    return true;
}

bool HotkeyFacade::registerSequence(const QString &text)
{
    if (!m_service)
        return false;
    quint32 modifiers = 0;
    quint32 virtualKey = 0;
    QString error;
    if (!parseSequence(text, &modifiers, &virtualKey, &error)) {
        setError(error, false);
        return false;
    }
    if (!m_service->setBinding(modifiers, virtualKey, &error)) {
        setError(error, true);
        return false;
    }
    QSettings settings;
    m_service->save(settings);
    setError(QString(), false);
    emit sequenceChanged();
    return true;
}

void HotkeyFacade::resetDefault()
{
    if (!m_service)
        return;
    QString error;
    if (m_service->registerDefault(&error)) {
        QSettings settings;
        m_service->save(settings);
        setError(QString(), false);
        emit sequenceChanged();
    } else {
        setError(error, true);
    }
}

void HotkeyFacade::setEnabled(bool value)
{
    if (!m_service)
        return;
    if (value) {
        QString error;
        QSettings settings;
        if (!m_service->loadAndRegister(settings, &error))
            setError(error, true);
    } else {
        m_service->disable();
    }
}

void HotkeyFacade::setError(const QString &message, bool conflictValue)
{
    if (m_errorMessage == message && m_conflict == conflictValue)
        return;
    m_errorMessage = message;
    m_conflict = conflictValue;
    emit errorChanged();
}
