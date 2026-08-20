#pragma once

#include <QObject>

namespace smartkey {

class ChatStorage;
class ProviderSettings;

class DataManagementService final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int activeChatCount READ activeChatCount NOTIFY statisticsChanged)
    Q_PROPERTY(int deletedChatCount READ deletedChatCount NOTIFY statisticsChanged)
    Q_PROPERTY(qint64 databaseBytes READ databaseBytes NOTIFY statisticsChanged)
    Q_PROPERTY(qint64 logBytes READ logBytes NOTIFY statisticsChanged)
    Q_PROPERTY(bool historyPersistenceEnabled READ historyPersistenceEnabled NOTIFY policyChanged)
    Q_PROPERTY(int retentionDays READ retentionDays NOTIFY policyChanged)
    Q_PROPERTY(QString operationMessage READ operationMessage NOTIFY operationChanged)
    Q_PROPERTY(bool operationSucceeded READ operationSucceeded NOTIFY operationChanged)

public:
    DataManagementService(ChatStorage *storage, ProviderSettings *providerSettings,
                          const QString &settingsFilePath, const QString &dataDirectory,
                          const QString &logFilePath, const QString &applicationVersion,
                          QObject *parent = nullptr);

    int activeChatCount() const { return m_activeChatCount; }
    int deletedChatCount() const { return m_deletedChatCount; }
    qint64 databaseBytes() const { return m_databaseBytes; }
    qint64 logBytes() const { return m_logBytes; }
    bool historyPersistenceEnabled() const { return m_historyPersistenceEnabled; }
    int retentionDays() const { return m_retentionDays; }
    QString operationMessage() const { return m_operationMessage; }
    bool operationSucceeded() const { return m_operationSucceeded; }

    Q_INVOKABLE void refreshStatistics();
    Q_INVOKABLE bool setHistoryPersistenceEnabled(bool enabled);
    Q_INVOKABLE bool setRetentionDays(int days);
    Q_INVOKABLE bool applyRetentionPolicy();
    Q_INVOKABLE bool exportAllChats(const QString &directoryUrl, const QString &format);
    Q_INVOKABLE bool exportDiagnostics(const QString &directoryUrl);
    Q_INVOKABLE bool openDataDirectory();
    Q_INVOKABLE bool clearLogs();
    Q_INVOKABLE bool clearAllChats();
    Q_INVOKABLE bool clearAllCredentials();

signals:
    void statisticsChanged();
    void policyChanged();
    void operationChanged();
    void chatsChanged();

private:
    void setOperation(bool succeeded, const QString &message);
    bool persistPolicy();
    QString localDirectory(const QString &pathOrUrl) const;
    qint64 databaseFileBytes() const;
    qint64 logFileBytes() const;

    ChatStorage *m_storage = nullptr;
    ProviderSettings *m_providerSettings = nullptr;
    QString m_settingsFilePath;
    QString m_dataDirectory;
    QString m_logFilePath;
    QString m_applicationVersion;
    int m_activeChatCount = 0;
    int m_deletedChatCount = 0;
    qint64 m_databaseBytes = 0;
    qint64 m_logBytes = 0;
    bool m_historyPersistenceEnabled = true;
    int m_retentionDays = 0;
    QString m_operationMessage;
    bool m_operationSucceeded = true;
};

} // namespace smartkey
