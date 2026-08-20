#pragma once

#include <QDateTime>
#include <QSqlDatabase>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

class QFileInfo;
class QSqlError;

namespace smartkey {

struct ConversationRecord
{
    QString id;
    QString title;
    QString providerProfileId;
    QString model;
    bool pinned = false;
    QDateTime createdAt;
    QDateTime updatedAt;
    QDateTime deletedAt;
};

struct ConversationQuery
{
    QString text;
    QString model;
    QDateTime from;
    QDateTime to;
    bool deletedOnly = false;
    bool includeDeleted = false;
    int limit = 50;
    int cursorPinned = -1;
    QString cursorUpdatedAt;
    QString cursorId;
};

struct ConversationPage
{
    QVariantList items;
    bool hasMore = false;
    int nextCursorPinned = -1;
    QString nextCursorUpdatedAt;
    QString nextCursorId;
};

struct MessageRecord
{
    QString id;
    QString conversationId;
    int ordinal = -1;
    QString role;
    QString content;
    QString reasoningContent;
    QString reference;
    QString status = QStringLiteral("complete");
    QString providerMessageId;
    QDateTime createdAt;
    QDateTime updatedAt;
};

// Deliberately contains operational metadata only. Prompt/response text,
// credentials and endpoint URLs have no representation in this record.
struct RequestAttemptRecord
{
    QString id;
    QString conversationId;
    QString messageId;
    QString providerProfileId;
    QString requestId;
    QString status = QStringLiteral("in_flight");
    QString errorCode;
    int httpStatus = 0;
    qint64 firstDeltaMs = -1;
    qint64 totalMs = -1;
    qint64 inputTokens = -1;
    qint64 outputTokens = -1;
    QDateTime createdAt;
};

struct LegacyImportResult
{
    bool success = false;
    bool alreadyImported = false;
    int conversationsImported = 0;
    int messagesImported = 0;
    QString errorMessage;
};

class ChatStorage final
{
public:
    static const int CurrentSchemaVersion = 4;
    static const int MaximumHistoryQueryLength = 200;
    static const int MaximumHistoryPageSize = 100;

    explicit ChatStorage(const QString &databasePath);
    ~ChatStorage();

    ChatStorage(const ChatStorage &) = delete;
    ChatStorage &operator=(const ChatStorage &) = delete;

    bool open();
    void close();
    bool checkpointAndClose();
    bool isOpen() const;
    bool historyPersistenceEnabled() const { return m_historyPersistenceEnabled; }
    void setHistoryPersistenceEnabled(bool enabled) { m_historyPersistenceEnabled = enabled; }
    QString databasePath() const { return m_databasePath; }
    QString errorString() const { return m_errorString; }
    int schemaVersion() const;

    bool upsertConversation(ConversationRecord record);
    bool appendMessage(MessageRecord record);
    // Persists the conversation, user message and assistant placeholder as one
    // unit. A failure leaves no partial turn behind.
    bool persistTurn(ConversationRecord conversation, MessageRecord userMessage,
                     MessageRecord assistantMessage);
    bool updateMessage(const QString &messageId, const QString &content,
                       const QString &reasoningContent, const QString &reference,
                       const QString &status);
    bool removeMessage(const QString &messageId);
    bool markIncompleteMessagesInterrupted();
    // Moves an active conversation to Recently Deleted. Physical deletion is
    // deliberately exposed separately and is never run during startup.
    bool removeConversation(const QString &conversationId);
    bool restoreConversation(const QString &conversationId);
    bool purgeConversation(const QString &conversationId);
    int purgeExpiredDeleted(int retentionDays = 7,
                            const QDateTime &now = QDateTime::currentDateTimeUtc());
    int softDeleteOlderThan(int retentionDays,
                            const QDateTime &now = QDateTime::currentDateTimeUtc());
    bool setConversationTitle(const QString &conversationId, const QString &title);
    bool setConversationPinned(const QString &conversationId, bool pinned);
    bool clearAllConversations();
    QVariantMap statistics() const;

    bool upsertRequestAttempt(RequestAttemptRecord record);
    QVariantList requestAttempts(const QString &conversationId = QString()) const;
    bool removeRequestAttempt(const QString &attemptId);

    QVariantList conversations() const;
    ConversationPage queryConversations(const ConversationQuery &query) const;
    QVariantMap conversation(const QString &conversationId,
                             bool includeDeleted = false) const;
    QVariantList messages(const QString &conversationId) const;
    LegacyImportResult importLegacyChatHistory(const QString &directoryOrOverallTablePath);

private:
    bool migrate();
    bool createLatestSchema();
    bool migrateVersion1To2();
    bool migrateVersion2To3();
    bool migrateVersion3To4();
    bool execute(const QString &sql);
    bool tableHasColumn(const QString &tableName, const QString &columnName) const;
    bool legacySourceWasImported(const QString &sourcePath) const;
    bool recordLegacyImport(const QString &sourcePath, const QFileInfo &sourceInfo,
                            int conversations, int messages);
    int nextOrdinal(const QString &conversationId) const;
    bool fail(const QString &message) const;
    void setLastError(const QString &prefix, const QSqlError &error) const;

    QString m_databasePath;
    QString m_connectionName;
    QSqlDatabase m_database;
    mutable QString m_errorString;
    bool m_historyPersistenceEnabled = true;
};

} // namespace smartkey
