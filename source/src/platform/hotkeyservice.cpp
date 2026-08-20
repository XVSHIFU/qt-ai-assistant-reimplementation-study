#include "hotkeyservice.h"

#include <QCoreApplication>
#include <QSettings>

#ifdef Q_OS_WIN
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#endif

namespace {
constexpr int kHotkeyIdA = 0x534A;
constexpr int kHotkeyIdB = 0x534B;
constexpr quint32 kModifierNoRepeat = 0x4000;
constexpr quint32 kAllowedModifiers = HotkeyService::Alt | HotkeyService::Control
        | HotkeyService::Shift | HotkeyService::Win;
}

HotkeyService::HotkeyService(QObject *parent)
    : QObject(parent)
{
    if (QCoreApplication::instance())
        QCoreApplication::instance()->installNativeEventFilter(this);
}

HotkeyService::~HotkeyService()
{
    disable();
    if (QCoreApplication::instance())
        QCoreApplication::instance()->removeNativeEventFilter(this);
}

bool HotkeyService::isSupported()
{
#ifdef Q_OS_WIN
    return true;
#else
    return false;
#endif
}

bool HotkeyService::registerDefault(QString *errorMessage)
{
    return setBinding(defaultModifiers(), defaultVirtualKey(), errorMessage);
}

bool HotkeyService::validateBinding(quint32 modifiers, quint32 virtualKey, QString *errorMessage)
{
    if ((modifiers & ~kAllowedModifiers) != 0) {
        if (errorMessage)
            *errorMessage = tr("快捷键包含不支持的修饰键。");
        return false;
    }
    if (virtualKey == 0 || virtualKey == 0x7B) { // VK_F12 is reserved by Windows debuggers.
        if (errorMessage)
            *errorMessage = tr("该按键不能注册为全局快捷键。");
        return false;
    }
    if (virtualKey == 0x10 || virtualKey == 0x11 || virtualKey == 0x12
            || virtualKey == 0x5B || virtualKey == 0x5C) {
        if (errorMessage)
            *errorMessage = tr("修饰键不能单独作为全局快捷键。");
        return false;
    }
    const bool ordinaryKey = (virtualKey >= 'A' && virtualKey <= 'Z')
            || (virtualKey >= '0' && virtualKey <= '9') || virtualKey == 0x20;
    const quint32 ordinaryModifiers = Alt | Control | Shift;
    if (ordinaryKey && (modifiers & ordinaryModifiers) == 0) {
        if (errorMessage)
            *errorMessage = tr("字母、数字和空格必须搭配 Ctrl、Alt 或 Shift。");
        return false;
    }
    return true;
}

bool HotkeyService::setBinding(quint32 modifiers, quint32 virtualKey, QString *errorMessage)
{
    QString validationError;
    if (!validateBinding(modifiers, virtualKey, &validationError)) {
        if (errorMessage)
            *errorMessage = validationError;
        emit registrationFailed(validationError);
        return false;
    }

    if (m_enabled && m_modifiers == modifiers && m_virtualKey == virtualKey)
        return true;

#ifdef Q_OS_WIN
    const int candidateId = m_activeId == kHotkeyIdA ? kHotkeyIdB : kHotkeyIdA;
    const UINT nativeModifiers = static_cast<UINT>(modifiers | kModifierNoRepeat);
    if (!RegisterHotKey(nullptr, candidateId, nativeModifiers, static_cast<UINT>(virtualKey))) {
        const DWORD code = GetLastError();
        QString message;
        if (code == ERROR_HOTKEY_ALREADY_REGISTERED)
            message = tr("快捷键已被其他应用占用，原快捷键保持不变。");
        else
            message = tr("注册全局快捷键失败，原快捷键保持不变：%1").arg(nativeErrorMessage(code));
        if (errorMessage)
            *errorMessage = message;
        emit registrationFailed(message);
        return false;
    }

    if (m_activeId != 0)
        UnregisterHotKey(nullptr, m_activeId);
    m_activeId = candidateId;
    m_modifiers = modifiers;
    m_virtualKey = virtualKey;
    setEnabled(true);
    emit bindingChanged(m_modifiers, m_virtualKey);
    return true;
#else
    const QString message = tr("当前平台不支持 Windows 全局快捷键。");
    if (errorMessage)
        *errorMessage = message;
    emit registrationFailed(message);
    return false;
#endif
}

void HotkeyService::disable()
{
#ifdef Q_OS_WIN
    if (m_activeId != 0)
        UnregisterHotKey(nullptr, m_activeId);
#endif
    m_activeId = 0;
    setEnabled(false);
}

bool HotkeyService::loadAndRegister(QSettings &settings, QString *errorMessage)
{
    settings.beginGroup(QStringLiteral("Hotkey"));
    const bool enabled = settings.value(QStringLiteral("enabled"), true).toBool();
    const quint32 storedModifiers = settings.value(QStringLiteral("modifiers"), defaultModifiers()).toUInt();
    const quint32 storedVirtualKey = settings.value(QStringLiteral("virtualKey"), defaultVirtualKey()).toUInt();
    settings.endGroup();

    if (!enabled) {
        disable();
        m_modifiers = storedModifiers;
        m_virtualKey = storedVirtualKey;
        emit bindingChanged(m_modifiers, m_virtualKey);
        return true;
    }
    return setBinding(storedModifiers, storedVirtualKey, errorMessage);
}

void HotkeyService::save(QSettings &settings) const
{
    settings.beginGroup(QStringLiteral("Hotkey"));
    settings.setValue(QStringLiteral("enabled"), m_enabled);
    settings.setValue(QStringLiteral("modifiers"), m_modifiers);
    settings.setValue(QStringLiteral("virtualKey"), m_virtualKey);
    settings.endGroup();
    settings.sync();
}

bool HotkeyService::nativeEventFilter(const QByteArray &eventType, void *message, long *result)
{
    Q_UNUSED(eventType)
    Q_UNUSED(result)
#ifdef Q_OS_WIN
    if (!m_enabled || !message)
        return false;
    const MSG *nativeMessage = static_cast<const MSG *>(message);
    if (nativeMessage->message == WM_HOTKEY
            && static_cast<int>(nativeMessage->wParam) == m_activeId) {
        emit activated();
        return true;
    }
#else
    Q_UNUSED(message)
#endif
    return false;
}

QString HotkeyService::nativeErrorMessage(unsigned long errorCode)
{
#ifdef Q_OS_WIN
    wchar_t *buffer = nullptr;
    const DWORD count = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER
                                       | FORMAT_MESSAGE_FROM_SYSTEM
                                       | FORMAT_MESSAGE_IGNORE_INSERTS,
                                       nullptr,
                                       static_cast<DWORD>(errorCode),
                                       0,
                                       reinterpret_cast<wchar_t *>(&buffer),
                                       0,
                                       nullptr);
    QString result = count && buffer
            ? QString::fromWCharArray(buffer, static_cast<int>(count)).trimmed()
            : tr("Windows 错误 %1").arg(errorCode);
    if (buffer)
        LocalFree(buffer);
    return result;
#else
    return tr("错误 %1").arg(errorCode);
#endif
}

void HotkeyService::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    emit enabledChanged(m_enabled);
}
