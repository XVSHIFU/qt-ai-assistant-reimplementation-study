#pragma once

#include <QObject>

class HotkeyService;

class HotkeyFacade final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString sequence READ sequence NOTIFY sequenceChanged)
    Q_PROPERTY(bool enabled READ enabled NOTIFY registeredChanged)
    Q_PROPERTY(bool registered READ registered NOTIFY registeredChanged)
    Q_PROPERTY(bool conflict READ conflict NOTIFY errorChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorChanged)

public:
    explicit HotkeyFacade(HotkeyService *service, QObject *parent = nullptr);

    QString sequence() const;
    bool enabled() const;
    bool registered() const;
    bool conflict() const { return m_conflict; }
    QString errorMessage() const { return m_errorMessage; }

    Q_INVOKABLE bool registerSequence(const QString &sequence);
    Q_INVOKABLE void resetDefault();
    Q_INVOKABLE void setEnabled(bool enabled);

signals:
    void activated();
    void sequenceChanged();
    void registeredChanged();
    void errorChanged();

private:
    static bool parseSequence(const QString &text, quint32 *modifiers,
                              quint32 *virtualKey, QString *errorMessage);
    void setError(const QString &message, bool conflict);

    HotkeyService *m_service = nullptr;
    QString m_errorMessage;
    bool m_conflict = false;
};
