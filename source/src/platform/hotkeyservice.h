#pragma once

#include <QAbstractNativeEventFilter>
#include <QObject>

class QSettings;

class HotkeyService final : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT
    Q_PROPERTY(bool enabled READ isEnabled NOTIFY enabledChanged)
    Q_PROPERTY(quint32 modifiers READ modifiers NOTIFY bindingChanged)
    Q_PROPERTY(quint32 virtualKey READ virtualKey NOTIFY bindingChanged)

public:
    enum Modifier : quint32 {
        Alt = 0x0001,
        Control = 0x0002,
        Shift = 0x0004,
        Win = 0x0008
    };
    Q_DECLARE_FLAGS(Modifiers, Modifier)
    Q_FLAG(Modifiers)

    explicit HotkeyService(QObject *parent = nullptr);
    ~HotkeyService() override;

    static quint32 defaultModifiers() { return Control | Alt; }
    static quint32 defaultVirtualKey() { return 0x20; } // VK_SPACE
    static bool isSupported();

    bool isEnabled() const { return m_enabled; }
    quint32 modifiers() const { return m_modifiers; }
    quint32 virtualKey() const { return m_virtualKey; }

    bool registerDefault(QString *errorMessage = nullptr);
    bool setBinding(quint32 modifiers, quint32 virtualKey, QString *errorMessage = nullptr);
    void disable();
    bool loadAndRegister(QSettings &settings, QString *errorMessage = nullptr);
    void save(QSettings &settings) const;

    bool nativeEventFilter(const QByteArray &eventType, void *message, long *result) override;

signals:
    void activated();
    void enabledChanged(bool enabled);
    void bindingChanged(quint32 modifiers, quint32 virtualKey);
    void registrationFailed(const QString &message);

private:
    static bool validateBinding(quint32 modifiers, quint32 virtualKey, QString *errorMessage);
    static QString nativeErrorMessage(unsigned long errorCode);
    void setEnabled(bool enabled);

    bool m_enabled = false;
    quint32 m_modifiers = defaultModifiers();
    quint32 m_virtualKey = defaultVirtualKey();
    int m_activeId = 0;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(HotkeyService::Modifiers)
