#pragma once

#include <QObject>

class AutoStartService final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ isAvailable CONSTANT)
    Q_PROPERTY(bool enabled READ isEnabled NOTIFY enabledChanged)

public:
    explicit AutoStartService(const QString &valueName = QString(), QObject *parent = nullptr);

    static bool isAvailable();
    QString valueName() const { return m_valueName; }
    bool isEnabled() const;
    QString startupCommand() const;
    QStringList startupArguments() const { return m_arguments; }
    void setStartupArguments(const QStringList &arguments) { m_arguments = arguments; }

public slots:
    bool setEnabled(bool enabled, QString *errorMessage = nullptr);

signals:
    void enabledChanged(bool enabled);
    void updateFailed(const QString &message);

private:
    QString m_valueName;
    QStringList m_arguments{QStringLiteral("--background")};
};
