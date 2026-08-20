#pragma once

#include <QObject>
#include <QString>

class QLocalServer;

class SingleInstanceService final : public QObject
{
    Q_OBJECT

public:
    enum AcquireResult {
        Primary,
        SecondaryNotified,
        Failed
    };
    Q_ENUM(AcquireResult)

    explicit SingleInstanceService(const QString &serverName = QString(), QObject *parent = nullptr);
    ~SingleInstanceService() override;

    static QString defaultServerName();
    static QString serverNameForUserIdentity(const QString &userIdentity);
    QString serverName() const { return m_serverName; }
    bool isPrimary() const { return m_primary; }

    AcquireResult acquire(const QString &message = QStringLiteral("activate"),
                          int timeoutMs = 1000,
                          QString *errorMessage = nullptr);
    bool notifyPrimary(const QString &message, int timeoutMs = 1000,
                       QString *errorMessage = nullptr) const;
    void close();

signals:
    void activationRequested();
    void messageReceived(const QString &message);

private slots:
    void acceptPendingConnections();

private:
    QString m_serverName;
    QLocalServer *m_server = nullptr;
    bool m_primary = false;
};
