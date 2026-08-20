#include "singleinstanceservice.h"

#include <QCryptographicHash>
#include <QLocalServer>
#include <QLocalSocket>

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

namespace {

// This identifier is part of the IPC protocol. Keep it stable across upgrades,
// install locations and executable renames.
const char kProductInstanceId[] = "smartkey-ai-desktop-7c075305-5c88-4cb5-865e-1426dc2accba";

QString currentUserIdentity()
{
#ifdef Q_OS_WIN
    HANDLE token = nullptr;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        DWORD requiredSize = 0;
        GetTokenInformation(token, TokenUser, nullptr, 0, &requiredSize);
        if (requiredSize > 0) {
            QByteArray tokenBuffer(static_cast<int>(requiredSize), '\0');
            if (GetTokenInformation(token,
                                    TokenUser,
                                    tokenBuffer.data(),
                                    requiredSize,
                                    &requiredSize)) {
                const TOKEN_USER *tokenUser =
                        reinterpret_cast<const TOKEN_USER *>(tokenBuffer.constData());
                if (tokenUser->User.Sid && IsValidSid(tokenUser->User.Sid)) {
                    const DWORD sidSize = GetLengthSid(tokenUser->User.Sid);
                    const QByteArray sidBytes(
                            reinterpret_cast<const char *>(tokenUser->User.Sid),
                            static_cast<int>(sidSize));
                    CloseHandle(token);
                    return QStringLiteral("sid:%1")
                            .arg(QString::fromLatin1(sidBytes.toHex()));
                }
            }
        }
        CloseHandle(token);
    }
#endif

    // SID lookup should only fail in a severely restricted Windows process.
    // Retain a deterministic per-account fallback for that case and for other
    // supported platforms.
    const QString domain = QString::fromLocal8Bit(qgetenv("USERDOMAIN"));
    QString user = QString::fromLocal8Bit(qgetenv("USERNAME"));
    if (user.isEmpty())
        user = QString::fromLocal8Bit(qgetenv("USER"));
    return QStringLiteral("account:%1\\%2").arg(domain, user);
}

} // namespace

SingleInstanceService::SingleInstanceService(const QString &serverName, QObject *parent)
    : QObject(parent),
      m_serverName(serverName.isEmpty() ? defaultServerName() : serverName),
      m_server(new QLocalServer(this))
{
    connect(m_server, &QLocalServer::newConnection,
            this, &SingleInstanceService::acceptPendingConnections);
}

SingleInstanceService::~SingleInstanceService()
{
    close();
}

QString SingleInstanceService::defaultServerName()
{
    return serverNameForUserIdentity(currentUserIdentity());
}

QString SingleInstanceService::serverNameForUserIdentity(const QString &userIdentity)
{
    const QByteArray identity = QByteArray(kProductInstanceId) + '\0'
            + userIdentity.trimmed().toUtf8();
    const QByteArray digest = QCryptographicHash::hash(identity, QCryptographicHash::Sha256)
            .toHex().left(24);
    return QStringLiteral("SmartKeyAI-7c075305-%1").arg(QString::fromLatin1(digest));
}

SingleInstanceService::AcquireResult SingleInstanceService::acquire(const QString &message,
                                                                    int timeoutMs,
                                                                    QString *errorMessage)
{
    if (m_primary)
        return Primary;

    QString notificationError;
    if (notifyPrimary(message, qMax(100, timeoutMs / 2), &notificationError))
        return SecondaryNotified;

    if (m_server->listen(m_serverName)) {
        m_primary = true;
        return Primary;
    }

    // Handle the race in which another instance began listening after the first probe.
    if (notifyPrimary(message, qMax(100, timeoutMs / 2), &notificationError))
        return SecondaryNotified;

    if (m_server->serverError() == QAbstractSocket::AddressInUseError) {
#ifndef Q_OS_WIN
        // Unix-domain sockets can leave a filesystem entry after an unclean exit.
        // Windows named pipes are removed by the OS; never steal a live Windows name.
        QLocalServer::removeServer(m_serverName);
        if (m_server->listen(m_serverName)) {
            m_primary = true;
            return Primary;
        }
#endif
    }

    const QString messageText = tr("无法创建单实例通道 %1：%2")
            .arg(m_serverName, m_server->errorString());
    if (errorMessage)
        *errorMessage = messageText;
    return Failed;
}

bool SingleInstanceService::notifyPrimary(const QString &message,
                                          int timeoutMs,
                                          QString *errorMessage) const
{
    QLocalSocket socket;
    socket.connectToServer(m_serverName, QIODevice::WriteOnly);
    if (!socket.waitForConnected(timeoutMs)) {
        if (errorMessage)
            *errorMessage = socket.errorString();
        return false;
    }

    QByteArray payload = message.toUtf8();
    payload.replace('\n', ' ');
    payload.append('\n');
    if (socket.write(payload) != payload.size() || !socket.waitForBytesWritten(timeoutMs)) {
        if (errorMessage)
            *errorMessage = socket.errorString();
        return false;
    }
    socket.disconnectFromServer();
    return true;
}

void SingleInstanceService::close()
{
    if (!m_server)
        return;
    if (m_server->isListening())
        m_server->close();
    m_primary = false;
}

void SingleInstanceService::acceptPendingConnections()
{
    while (m_server->hasPendingConnections()) {
        QLocalSocket *socket = m_server->nextPendingConnection();
        if (!socket)
            continue;
        socket->setParent(this);

        const auto consume = [this, socket] {
            while (socket->canReadLine()) {
                const QString message = QString::fromUtf8(socket->readLine()).trimmed();
                if (message.isEmpty())
                    continue;
                emit messageReceived(message);
                if (message == QStringLiteral("activate"))
                    emit activationRequested();
            }
        };
        connect(socket, &QLocalSocket::readyRead, this, consume);
        connect(socket, &QLocalSocket::disconnected, this, [consume, socket] {
            consume();
            socket->deleteLater();
        });
        consume();
    }
}
