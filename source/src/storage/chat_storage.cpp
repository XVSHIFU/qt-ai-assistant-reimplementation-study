#include "chat_storage.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

namespace smartkey {
namespace {

QString uuid()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString isoUtc(const QDateTime &value)
{
    const QDateTime normalized = value.isValid() ? value.toUTC() : QDateTime::currentDateTimeUtc();
    return normalized.toString(Qt::ISODateWithMs);
}

QString nonNull(QString value)
{
    if (value.isNull())
        value = QStringLiteral("");
    return value;
}

QString escapeLike(QString value)
{
    value.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    value.replace(QLatin1Char('%'), QStringLiteral("\\%"));
    value.replace(QLatin1Char('_'), QStringLiteral("\\_"));
    return value;
}

QDateTime parseLegacyTime(const QVariant &value)
{
    if (value.userType() == QMetaType::LongLong || value.userType() == QMetaType::ULongLong
        || value.userType() == QMetaType::Int || value.userType() == QMetaType::UInt) {
        qint64 number = value.toLongLong();
        if (number > 0 && number < 100000000000LL)
            number *= 1000;
        return QDateTime::fromMSecsSinceEpoch(number, Qt::UTC);
    }
    const QString text = value.toString().trimmed();
    QDateTime parsed = QDateTime::fromString(text, Qt::ISODateWithMs);
    if (!parsed.isValid())
        parsed = QDateTime::fromString(text, Qt::ISODate);
    if (!parsed.isValid())
        parsed = QDateTime::fromString(text, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    return parsed.isValid() ? parsed.toUTC() : QDateTime::currentDateTimeUtc();
}

QString jsonValueAsText(const QVariant &value)
{
    if (value.type() == QVariant::List || value.type() == QVariant::Map)
        return QString::fromUtf8(QJsonDocument::fromVariant(value).toJson(QJsonDocument::Compact));
    return value.toString();
}

QVariantList historyItems(const QVariant &root)
{
    if (root.type() == QVariant::List)
        return root.toList();
    const QVariantMap object = root.toMap();
    if (object.value(QStringLiteral("history")).type() == QVariant::List)
        return object.value(QStringLiteral("history")).toList();
    if (object.contains(QStringLiteral("chat")) || object.contains(QStringLiteral("messages")))
        return {object};
    return {};
}

QVariantList chatItems(const QVariantMap &conversation)
{
    const QVariant chat = conversation.contains(QStringLiteral("chat"))
        ? conversation.value(QStringLiteral("chat"))
        : conversation.value(QStringLiteral("messages"));
    if (chat.type() == QVariant::List)
        return chat.toList();
    return {};
}

QString roleFromLegacySender(const QVariant &sender)
{
    const QString text = sender.toString().trimmed().toLower();
    if (text == QLatin1String("0") || text == QLatin1String("user")
        || text == QLatin1String("human"))
        return QStringLiteral("user");
    if (text == QLatin1String("system"))
        return QStringLiteral("system");
    return QStringLiteral("assistant");
}

bool isPathInside(const QString &filePath, const QString &directoryPath)
{
    QString directory = QDir::fromNativeSeparators(
        QDir::cleanPath(QFileInfo(directoryPath).absoluteFilePath()));
    if (!directory.endsWith(QLatin1Char('/')))
        directory.append(QLatin1Char('/'));
    const QString file = QDir::fromNativeSeparators(
        QDir::cleanPath(QFileInfo(filePath).absoluteFilePath()));
    return file.startsWith(directory, Qt::CaseInsensitive);
}

bool safeAttemptIdentifier(const QString &value, bool allowEmpty = false)
{
    if (value.isEmpty())
        return allowEmpty;
    static const QRegularExpression safe(QStringLiteral("^[A-Za-z0-9_-]{1,128}$"));
    return safe.match(value).hasMatch();
}

bool safeAttemptCategory(const QString &value, bool allowEmpty = false)
{
    if (value.isEmpty())
        return allowEmpty;
    static const QRegularExpression safe(QStringLiteral("^[a-z0-9_]{1,64}$"));
    return safe.match(value).hasMatch();
}

} // namespace

ChatStorage::ChatStorage(const QString &databasePath)
    : m_databasePath(QFileInfo(databasePath).absoluteFilePath()),
      m_connectionName(QStringLiteral("SmartKeyChatStorage-%1").arg(uuid()))
{
}

ChatStorage::~ChatStorage()
{
    close();
}

bool ChatStorage::open()
{
    if (m_database.isOpen()) {
        m_errorString.clear();
        return true;
    }
    const QString parentPath = QFileInfo(m_databasePath).absolutePath();
    if (!QDir().mkpath(parentPath))
        return fail(QStringLiteral("Open database: unable to create directory %1").arg(parentPath));
    m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_database.setDatabaseName(m_databasePath);
    m_database.setConnectOptions(QStringLiteral("QSQLITE_BUSY_TIMEOUT=5000"));
    if (!m_database.open()) {
        setLastError(QStringLiteral("Open database"), m_database.lastError());
        return false;
    }
    if (!execute(QStringLiteral("PRAGMA foreign_keys = ON"))
        || !execute(QStringLiteral("PRAGMA journal_mode = WAL"))
        || !migrate()) {
        close();
        return false;
    }
    m_errorString.clear();
    return true;
}

void ChatStorage::close()
{
    if (!m_database.isValid())
        return;
    m_database.close();
    m_database = QSqlDatabase();
    QSqlDatabase::removeDatabase(m_connectionName);
}

bool ChatStorage::checkpointAndClose()
{
    if (!m_database.isOpen()) {
        close();
        return true;
    }

    bool checkpointed = false;
    QSqlQuery query(m_database);
    if (!query.exec(QStringLiteral("PRAGMA wal_checkpoint(TRUNCATE)"))) {
        setLastError(QStringLiteral("Checkpoint database"), query.lastError());
    } else if (!query.next() || query.value(0).toInt() != 0) {
        m_errorString = QStringLiteral("Checkpoint database: SQLite reported a busy WAL");
    } else {
        checkpointed = true;
        m_errorString.clear();
    }
    close();
    return checkpointed;
}

bool ChatStorage::isOpen() const
{
    return m_database.isOpen();
}

int ChatStorage::schemaVersion() const
{
    if (!m_database.isOpen())
        return -1;
    QSqlQuery query(m_database);
    if (!query.exec(QStringLiteral("PRAGMA user_version")) || !query.next())
        return -1;
    return query.value(0).toInt();
}

bool ChatStorage::migrate()
{
    const int version = schemaVersion();
    if (version < 0) {
        m_errorString = QStringLiteral("Unable to read SQLite schema version");
        return false;
    }
    if (version > CurrentSchemaVersion) {
        m_errorString = QStringLiteral("Database schema %1 is newer than supported schema %2")
                            .arg(version).arg(CurrentSchemaVersion);
        return false;
    }
    if (version == CurrentSchemaVersion)
        return true;
    if (!m_database.transaction()) {
        setLastError(QStringLiteral("Begin migration"), m_database.lastError());
        return false;
    }
    bool ok = version == 0 ? createLatestSchema() : true;
    if (ok && version == 1)
        ok = migrateVersion1To2();
    if (ok && version <= 2 && version > 0)
        ok = migrateVersion2To3();
    if (ok && version <= 3 && version > 0)
        ok = migrateVersion3To4();
    if (ok)
        ok = execute(QStringLiteral("PRAGMA user_version = %1").arg(CurrentSchemaVersion));
    if (!ok || !m_database.commit()) {
        if (ok)
            setLastError(QStringLiteral("Commit migration"), m_database.lastError());
        m_database.rollback();
        return false;
    }
    return true;
}

bool ChatStorage::createLatestSchema()
{
    return execute(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS conversations ("
        "id TEXT PRIMARY KEY, title TEXT NOT NULL, provider_profile_id TEXT NOT NULL DEFAULT '', "
        "model TEXT NOT NULL DEFAULT '', pinned INTEGER NOT NULL DEFAULT 0, "
        "created_at TEXT NOT NULL, updated_at TEXT NOT NULL, deleted_at TEXT NOT NULL DEFAULT '')"))
        && execute(QStringLiteral(
        "CREATE INDEX IF NOT EXISTS idx_conversations_history "
        "ON conversations(deleted_at,pinned DESC,updated_at DESC,id DESC)"))
        && execute(QStringLiteral(
        "CREATE INDEX IF NOT EXISTS idx_conversations_model "
        "ON conversations(model,deleted_at,updated_at DESC)"))
        && execute(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS messages ("
        "id TEXT PRIMARY KEY, conversation_id TEXT NOT NULL, ordinal INTEGER NOT NULL, "
        "role TEXT NOT NULL CHECK(role IN ('system','user','assistant','tool')), "
        "content TEXT NOT NULL DEFAULT '', reasoning_content TEXT NOT NULL DEFAULT '', "
        "reference_text TEXT NOT NULL DEFAULT '', status TEXT NOT NULL DEFAULT 'complete', "
        "provider_message_id TEXT NOT NULL DEFAULT '', created_at TEXT NOT NULL, updated_at TEXT NOT NULL, "
        "UNIQUE(conversation_id, ordinal), "
        "FOREIGN KEY(conversation_id) REFERENCES conversations(id) ON DELETE CASCADE)"))
        && execute(QStringLiteral(
        "CREATE INDEX IF NOT EXISTS idx_messages_conversation "
        "ON messages(conversation_id, ordinal)"))
        && execute(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS request_attempts ("
        "id TEXT PRIMARY KEY, conversation_id TEXT NOT NULL DEFAULT '', message_id TEXT NOT NULL DEFAULT '', "
        "provider_profile_id TEXT NOT NULL DEFAULT '', request_id TEXT NOT NULL DEFAULT '', "
        "status TEXT NOT NULL, error_code TEXT NOT NULL DEFAULT '', http_status INTEGER NOT NULL DEFAULT 0, "
        "first_delta_ms INTEGER NOT NULL DEFAULT -1, total_ms INTEGER NOT NULL DEFAULT -1, "
        "input_tokens INTEGER NOT NULL DEFAULT -1, output_tokens INTEGER NOT NULL DEFAULT -1, "
        "created_at TEXT NOT NULL)"))
        && execute(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS legacy_imports ("
        "source_path TEXT PRIMARY KEY, source_size INTEGER NOT NULL, source_modified_at TEXT NOT NULL, "
        "imported_at TEXT NOT NULL, conversations_imported INTEGER NOT NULL, "
        "messages_imported INTEGER NOT NULL, status TEXT NOT NULL)"));
}

bool ChatStorage::migrateVersion1To2()
{
    struct Column { const char *table; const char *name; const char *definition; };
    const Column additions[] = {
        {"conversations", "provider_profile_id", "TEXT NOT NULL DEFAULT ''"},
        {"conversations", "model", "TEXT NOT NULL DEFAULT ''"},
        {"conversations", "pinned", "INTEGER NOT NULL DEFAULT 0"},
        {"conversations", "updated_at", "TEXT NOT NULL DEFAULT ''"},
        {"conversations", "deleted_at", "TEXT NOT NULL DEFAULT ''"},
        {"messages", "reasoning_content", "TEXT NOT NULL DEFAULT ''"},
        {"messages", "reference_text", "TEXT NOT NULL DEFAULT ''"},
        {"messages", "status", "TEXT NOT NULL DEFAULT 'complete'"},
        {"messages", "provider_message_id", "TEXT NOT NULL DEFAULT ''"},
        {"messages", "updated_at", "TEXT NOT NULL DEFAULT ''"}
    };
    for (const Column &column : additions) {
        if (!tableHasColumn(QLatin1String(column.table), QLatin1String(column.name))
            && !execute(QStringLiteral("ALTER TABLE %1 ADD COLUMN %2 %3")
                            .arg(QLatin1String(column.table), QLatin1String(column.name),
                                 QLatin1String(column.definition))))
            return false;
    }
    return createLatestSchema()
        && execute(QStringLiteral("UPDATE conversations SET updated_at = created_at WHERE updated_at = ''"))
        && execute(QStringLiteral("UPDATE messages SET updated_at = created_at WHERE updated_at = ''"));
}

bool ChatStorage::migrateVersion2To3()
{
    if (!tableHasColumn(QStringLiteral("conversations"), QStringLiteral("pinned"))
        && !execute(QStringLiteral(
            "ALTER TABLE conversations ADD COLUMN pinned INTEGER NOT NULL DEFAULT 0")))
        return false;
    if (!tableHasColumn(QStringLiteral("conversations"), QStringLiteral("deleted_at"))
        && !execute(QStringLiteral(
            "ALTER TABLE conversations ADD COLUMN deleted_at TEXT NOT NULL DEFAULT ''")))
        return false;
    return createLatestSchema();
}

bool ChatStorage::migrateVersion3To4()
{
    if (!tableHasColumn(QStringLiteral("conversations"), QStringLiteral("deleted_at"))
        && !execute(QStringLiteral(
            "ALTER TABLE conversations ADD COLUMN deleted_at TEXT NOT NULL DEFAULT ''")))
        return false;
    return createLatestSchema();
}

bool ChatStorage::upsertConversation(ConversationRecord record)
{
    if (!isOpen())
        return fail(QStringLiteral("Save conversation: database is not open"));
    if (!m_historyPersistenceEnabled)
        return fail(QStringLiteral("Save conversation: history persistence is disabled"));
    if (record.id.isEmpty())
        record.id = uuid();
    if (!record.createdAt.isValid())
        record.createdAt = QDateTime::currentDateTimeUtc();
    if (!record.updatedAt.isValid())
        record.updatedAt = record.createdAt;

    QSqlQuery exists(m_database);
    exists.prepare(QStringLiteral("SELECT 1 FROM conversations WHERE id = ?"));
    exists.addBindValue(record.id);
    if (!exists.exec()) {
        setLastError(QStringLiteral("Check conversation"), exists.lastError());
        return false;
    }

    QSqlQuery query(m_database);
    if (exists.next()) {
        query.prepare(QStringLiteral(
            "UPDATE conversations SET title=?, provider_profile_id=?, model=?, pinned=?, updated_at=? WHERE id=?"));
        query.addBindValue(nonNull(record.title));
        query.addBindValue(nonNull(record.providerProfileId));
        query.addBindValue(nonNull(record.model));
        query.addBindValue(record.pinned ? 1 : 0);
        query.addBindValue(isoUtc(record.updatedAt));
        query.addBindValue(record.id);
    } else {
        query.prepare(QStringLiteral(
            "INSERT INTO conversations(id,title,provider_profile_id,model,pinned,created_at,updated_at,deleted_at) "
            "VALUES(?,?,?,?,?,?,?,?)"));
        query.addBindValue(record.id);
        query.addBindValue(nonNull(record.title));
        query.addBindValue(nonNull(record.providerProfileId));
        query.addBindValue(nonNull(record.model));
        query.addBindValue(record.pinned ? 1 : 0);
        query.addBindValue(isoUtc(record.createdAt));
        query.addBindValue(isoUtc(record.updatedAt));
        query.addBindValue(record.deletedAt.isValid() ? isoUtc(record.deletedAt) : QStringLiteral(""));
    }
    if (!query.exec()) {
        setLastError(QStringLiteral("Save conversation"), query.lastError());
        return false;
    }
    m_errorString.clear();
    return true;
}

bool ChatStorage::appendMessage(MessageRecord record)
{
    if (!isOpen())
        return fail(QStringLiteral("Append message: database is not open"));
    if (record.conversationId.isEmpty())
        return fail(QStringLiteral("Append message: conversation id is empty"));
    if (record.id.isEmpty())
        record.id = uuid();
    if (record.ordinal < 0)
        record.ordinal = nextOrdinal(record.conversationId);
    if (record.ordinal < 0)
        return fail(QStringLiteral("Append message: unable to determine message ordinal"));
    if (!record.createdAt.isValid())
        record.createdAt = QDateTime::currentDateTimeUtc();
    if (!record.updatedAt.isValid())
        record.updatedAt = record.createdAt;

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "INSERT INTO messages(id,conversation_id,ordinal,role,content,reasoning_content,"
        "reference_text,status,provider_message_id,created_at,updated_at) VALUES(?,?,?,?,?,?,?,?,?,?,?)"));
    query.addBindValue(record.id);
    query.addBindValue(record.conversationId);
    query.addBindValue(record.ordinal);
    query.addBindValue(nonNull(record.role));
    query.addBindValue(nonNull(record.content));
    query.addBindValue(nonNull(record.reasoningContent));
    query.addBindValue(nonNull(record.reference));
    query.addBindValue(nonNull(record.status));
    query.addBindValue(nonNull(record.providerMessageId));
    query.addBindValue(isoUtc(record.createdAt));
    query.addBindValue(isoUtc(record.updatedAt));
    if (!query.exec()) {
        setLastError(QStringLiteral("Append message"), query.lastError());
        return false;
    }
    m_errorString.clear();
    return true;
}

bool ChatStorage::persistTurn(ConversationRecord conversation, MessageRecord userMessage,
                              MessageRecord assistantMessage)
{
    if (!isOpen())
        return fail(QStringLiteral("Persist turn: database is not open"));
    if (!m_historyPersistenceEnabled)
        return fail(QStringLiteral("Persist turn: history persistence is disabled"));
    if (conversation.id.isEmpty() || userMessage.id.isEmpty() || assistantMessage.id.isEmpty())
        return fail(QStringLiteral("Persist turn: record id is empty"));
    if (userMessage.conversationId != conversation.id
        || assistantMessage.conversationId != conversation.id
        || userMessage.role != QLatin1String("user")
        || assistantMessage.role != QLatin1String("assistant")) {
        return fail(QStringLiteral("Persist turn: invalid conversation or message roles"));
    }
    if (!m_database.transaction()) {
        setLastError(QStringLiteral("Begin turn transaction"), m_database.lastError());
        return false;
    }
    if (!upsertConversation(conversation)
        || !appendMessage(userMessage)
        || !appendMessage(assistantMessage)) {
        const QString failure = m_errorString;
        m_database.rollback();
        m_errorString = failure;
        return false;
    }
    if (!m_database.commit()) {
        setLastError(QStringLiteral("Commit turn transaction"), m_database.lastError());
        const QString failure = m_errorString;
        m_database.rollback();
        m_errorString = failure;
        return false;
    }
    m_errorString.clear();
    return true;
}

bool ChatStorage::updateMessage(const QString &messageId, const QString &content,
                                const QString &reasoningContent, const QString &reference,
                                const QString &status)
{
    if (!isOpen())
        return fail(QStringLiteral("Update message: database is not open"));
    if (messageId.isEmpty())
        return fail(QStringLiteral("Update message: message id is empty"));
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "UPDATE messages SET content=?,reasoning_content=?,reference_text=?,status=?,updated_at=? WHERE id=?"));
    query.addBindValue(nonNull(content));
    query.addBindValue(nonNull(reasoningContent));
    query.addBindValue(nonNull(reference));
    query.addBindValue(nonNull(status));
    query.addBindValue(isoUtc(QDateTime::currentDateTimeUtc()));
    query.addBindValue(messageId);
    if (!query.exec()) {
        setLastError(QStringLiteral("Update message"), query.lastError());
        return false;
    }
    if (query.numRowsAffected() != 1)
        return fail(QStringLiteral("Update message: message was not found"));
    m_errorString.clear();
    return true;
}

bool ChatStorage::removeMessage(const QString &messageId)
{
    if (!isOpen())
        return fail(QStringLiteral("Remove message: database is not open"));
    if (messageId.isEmpty())
        return fail(QStringLiteral("Remove message: message id is empty"));
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("DELETE FROM messages WHERE id=?"));
    query.addBindValue(messageId);
    if (!query.exec()) {
        setLastError(QStringLiteral("Remove message"), query.lastError());
        return false;
    }
    if (query.numRowsAffected() != 1)
        return fail(QStringLiteral("Remove message: message was not found"));
    m_errorString.clear();
    return true;
}

bool ChatStorage::markIncompleteMessagesInterrupted()
{
    if (!isOpen())
        return fail(QStringLiteral("Recover incomplete messages: database is not open"));
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "UPDATE messages SET status='interrupted',updated_at=? "
        "WHERE status IN ('in_flight','streaming')"));
    query.addBindValue(isoUtc(QDateTime::currentDateTimeUtc()));
    if (!query.exec()) {
        setLastError(QStringLiteral("Recover incomplete messages"), query.lastError());
        return false;
    }
    m_errorString.clear();
    return true;
}

bool ChatStorage::removeConversation(const QString &conversationId)
{
    if (!isOpen())
        return fail(QStringLiteral("Remove conversation: database is not open"));
    if (conversationId.isEmpty())
        return fail(QStringLiteral("Remove conversation: conversation id is empty"));
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "UPDATE conversations SET deleted_at=?,pinned=0,updated_at=? "
        "WHERE id=? AND deleted_at=''"));
    const QString deletedAt = isoUtc(QDateTime::currentDateTimeUtc());
    query.addBindValue(deletedAt);
    query.addBindValue(deletedAt);
    query.addBindValue(conversationId);
    if (!query.exec()) {
        setLastError(QStringLiteral("Remove conversation"), query.lastError());
        return false;
    }
    if (query.numRowsAffected() != 1)
        return fail(QStringLiteral("Remove conversation: conversation was not found"));
    m_errorString.clear();
    return true;
}

bool ChatStorage::restoreConversation(const QString &conversationId)
{
    if (!isOpen())
        return fail(QStringLiteral("Restore conversation: database is not open"));
    if (conversationId.isEmpty())
        return fail(QStringLiteral("Restore conversation: conversation id is empty"));
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "UPDATE conversations SET deleted_at='',updated_at=? WHERE id=? AND deleted_at<>''"));
    query.addBindValue(isoUtc(QDateTime::currentDateTimeUtc()));
    query.addBindValue(conversationId);
    if (!query.exec()) {
        setLastError(QStringLiteral("Restore conversation"), query.lastError());
        return false;
    }
    if (query.numRowsAffected() != 1)
        return fail(QStringLiteral("Restore conversation: conversation was not found"));
    m_errorString.clear();
    return true;
}

bool ChatStorage::purgeConversation(const QString &conversationId)
{
    if (!isOpen())
        return fail(QStringLiteral("Purge conversation: database is not open"));
    if (conversationId.isEmpty())
        return fail(QStringLiteral("Purge conversation: conversation id is empty"));
    if (!m_database.transaction()) {
        setLastError(QStringLiteral("Begin purge conversation"), m_database.lastError());
        return false;
    }
    QSqlQuery attempts(m_database);
    attempts.prepare(QStringLiteral(
        "DELETE FROM request_attempts WHERE conversation_id=? AND EXISTS ("
        "SELECT 1 FROM conversations WHERE id=? AND deleted_at<>'')"));
    attempts.addBindValue(conversationId);
    attempts.addBindValue(conversationId);
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("DELETE FROM conversations WHERE id=? AND deleted_at<>''"));
    query.addBindValue(conversationId);
    if (!attempts.exec() || !query.exec() || query.numRowsAffected() != 1
            || !m_database.commit()) {
        if (query.lastError().isValid())
            setLastError(QStringLiteral("Purge conversation"), query.lastError());
        else
            m_errorString = QStringLiteral("Purge conversation: conversation was not found");
        m_database.rollback();
        return false;
    }
    m_errorString.clear();
    return true;
}

int ChatStorage::purgeExpiredDeleted(int retentionDays, const QDateTime &now)
{
    if (!isOpen()) {
        fail(QStringLiteral("Purge expired conversations: database is not open"));
        return -1;
    }
    if (retentionDays < 1 || retentionDays > 3650 || !now.isValid()) {
        fail(QStringLiteral("Purge expired conversations: invalid retention policy"));
        return -1;
    }
    if (!m_database.transaction()) {
        setLastError(QStringLiteral("Begin purge expired conversations"), m_database.lastError());
        return -1;
    }
    const QString threshold = isoUtc(now.toUTC().addDays(-retentionDays));
    QSqlQuery attempts(m_database);
    attempts.prepare(QStringLiteral(
        "DELETE FROM request_attempts WHERE conversation_id IN ("
        "SELECT id FROM conversations WHERE deleted_at<>'' AND deleted_at<=?)"));
    attempts.addBindValue(threshold);
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "DELETE FROM conversations WHERE deleted_at<>'' AND deleted_at<=?"));
    query.addBindValue(threshold);
    if (!attempts.exec() || !query.exec()) {
        setLastError(QStringLiteral("Purge expired conversations"), query.lastError());
        m_database.rollback();
        return -1;
    }
    const int removed = query.numRowsAffected();
    if (!m_database.commit()) {
        setLastError(QStringLiteral("Commit purge expired conversations"), m_database.lastError());
        m_database.rollback();
        return -1;
    }
    m_errorString.clear();
    return removed;
}

int ChatStorage::softDeleteOlderThan(int retentionDays, const QDateTime &now)
{
    if (!isOpen()) {
        fail(QStringLiteral("Apply retention policy: database is not open"));
        return -1;
    }
    if (retentionDays < 1 || retentionDays > 3650 || !now.isValid()) {
        fail(QStringLiteral("Apply retention policy: invalid retention policy"));
        return -1;
    }
    const QString changedAt = isoUtc(now);
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "UPDATE conversations SET deleted_at=?,pinned=0,updated_at=? "
        "WHERE deleted_at='' AND updated_at<?"));
    query.addBindValue(changedAt);
    query.addBindValue(changedAt);
    query.addBindValue(isoUtc(now.toUTC().addDays(-retentionDays)));
    if (!query.exec()) {
        setLastError(QStringLiteral("Apply retention policy"), query.lastError());
        return -1;
    }
    m_errorString.clear();
    return query.numRowsAffected();
}

bool ChatStorage::setConversationTitle(const QString &conversationId, const QString &title)
{
    if (!isOpen())
        return fail(QStringLiteral("Rename conversation: database is not open"));
    if (conversationId.isEmpty())
        return fail(QStringLiteral("Rename conversation: conversation id is empty"));
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "UPDATE conversations SET title=?,updated_at=? WHERE id=? AND deleted_at=''"));
    query.addBindValue(nonNull(title));
    query.addBindValue(isoUtc(QDateTime::currentDateTimeUtc()));
    query.addBindValue(conversationId);
    if (!query.exec()) {
        setLastError(QStringLiteral("Rename conversation"), query.lastError());
        return false;
    }
    if (query.numRowsAffected() != 1)
        return fail(QStringLiteral("Rename conversation: conversation was not found"));
    m_errorString.clear();
    return true;
}

bool ChatStorage::setConversationPinned(const QString &conversationId, bool pinned)
{
    if (!isOpen())
        return fail(QStringLiteral("Pin conversation: database is not open"));
    if (conversationId.isEmpty())
        return fail(QStringLiteral("Pin conversation: conversation id is empty"));
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("UPDATE conversations SET pinned=? WHERE id=? AND deleted_at=''"));
    query.addBindValue(pinned ? 1 : 0);
    query.addBindValue(conversationId);
    if (!query.exec()) {
        setLastError(QStringLiteral("Pin conversation"), query.lastError());
        return false;
    }
    if (query.numRowsAffected() != 1)
        return fail(QStringLiteral("Pin conversation: conversation was not found"));
    m_errorString.clear();
    return true;
}

bool ChatStorage::clearAllConversations()
{
    if (!isOpen())
        return fail(QStringLiteral("Clear conversations: database is not open"));
    if (!m_database.transaction()) {
        setLastError(QStringLiteral("Begin clear conversations"), m_database.lastError());
        return false;
    }
    QSqlQuery attempts(m_database);
    QSqlQuery conversations(m_database);
    if (!attempts.exec(QStringLiteral("DELETE FROM request_attempts"))
            || !conversations.exec(QStringLiteral("DELETE FROM conversations"))
            || !m_database.commit()) {
        const QSqlError error = conversations.lastError().isValid()
                ? conversations.lastError() : attempts.lastError();
        if (error.isValid())
            setLastError(QStringLiteral("Clear conversations"), error);
        else
            m_errorString = QStringLiteral("Clear conversations: commit failed");
        m_database.rollback();
        return false;
    }
    m_errorString.clear();
    return true;
}

QVariantMap ChatStorage::statistics() const
{
    QVariantMap result{{QStringLiteral("open"), isOpen()},
                       {QStringLiteral("schemaVersion"), schemaVersion()},
                       {QStringLiteral("activeConversations"), 0},
                       {QStringLiteral("deletedConversations"), 0},
                       {QStringLiteral("messages"), 0},
                       {QStringLiteral("requestAttempts"), 0}};
    if (!isOpen()) {
        fail(QStringLiteral("Read storage statistics: database is not open"));
        return result;
    }
    QSqlQuery query(m_database);
    if (!query.exec(QStringLiteral(
        "SELECT "
        "SUM(CASE WHEN deleted_at='' THEN 1 ELSE 0 END),"
        "SUM(CASE WHEN deleted_at<>'' THEN 1 ELSE 0 END) FROM conversations"))
            || !query.next()) {
        setLastError(QStringLiteral("Read conversation statistics"), query.lastError());
        return result;
    }
    result.insert(QStringLiteral("activeConversations"), query.value(0).toInt());
    result.insert(QStringLiteral("deletedConversations"), query.value(1).toInt());
    if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM messages")) || !query.next()) {
        setLastError(QStringLiteral("Read message statistics"), query.lastError());
        return result;
    }
    result.insert(QStringLiteral("messages"), query.value(0).toInt());
    if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM request_attempts")) || !query.next()) {
        setLastError(QStringLiteral("Read request attempt statistics"), query.lastError());
        return result;
    }
    result.insert(QStringLiteral("requestAttempts"), query.value(0).toInt());
    m_errorString.clear();
    return result;
}

bool ChatStorage::upsertRequestAttempt(RequestAttemptRecord record)
{
    if (!isOpen())
        return fail(QStringLiteral("Save request attempt: database is not open"));
    static const QStringList allowedStatuses = {
        QStringLiteral("in_flight"), QStringLiteral("completed"),
        QStringLiteral("failed"), QStringLiteral("cancelled")
    };
    if (!safeAttemptIdentifier(record.id)
            || !safeAttemptIdentifier(record.conversationId, true)
            || !safeAttemptIdentifier(record.messageId, true)
            || !safeAttemptIdentifier(record.providerProfileId, true)
            || !safeAttemptIdentifier(record.requestId, true)
            || !allowedStatuses.contains(record.status)
            || !safeAttemptCategory(record.errorCode, true)
            || record.httpStatus < 0 || record.httpStatus > 999
            || record.firstDeltaMs < -1 || record.totalMs < -1
            || record.inputTokens < -1 || record.outputTokens < -1) {
        return fail(QStringLiteral("Save request attempt: invalid metadata"));
    }
    if (!record.createdAt.isValid())
        record.createdAt = QDateTime::currentDateTimeUtc();

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO request_attempts("
        "id,conversation_id,message_id,provider_profile_id,request_id,status,error_code,"
        "http_status,first_delta_ms,total_ms,input_tokens,output_tokens,created_at) "
        "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?)"));
    query.addBindValue(record.id);
    query.addBindValue(nonNull(record.conversationId));
    query.addBindValue(nonNull(record.messageId));
    query.addBindValue(nonNull(record.providerProfileId));
    query.addBindValue(nonNull(record.requestId));
    query.addBindValue(record.status);
    query.addBindValue(nonNull(record.errorCode));
    query.addBindValue(record.httpStatus);
    query.addBindValue(record.firstDeltaMs);
    query.addBindValue(record.totalMs);
    query.addBindValue(record.inputTokens);
    query.addBindValue(record.outputTokens);
    query.addBindValue(isoUtc(record.createdAt));
    if (!query.exec()) {
        setLastError(QStringLiteral("Save request attempt"), query.lastError());
        return false;
    }
    m_errorString.clear();
    return true;
}

QVariantList ChatStorage::requestAttempts(const QString &conversationId) const
{
    QVariantList result;
    if (!isOpen()) {
        fail(QStringLiteral("Read request attempts: database is not open"));
        return result;
    }
    QSqlQuery query(m_database);
    if (conversationId.isEmpty()) {
        query.prepare(QStringLiteral(
            "SELECT id,conversation_id,message_id,provider_profile_id,request_id,status,"
            "error_code,http_status,first_delta_ms,total_ms,input_tokens,output_tokens,created_at "
            "FROM request_attempts ORDER BY created_at,id"));
    } else {
        query.prepare(QStringLiteral(
            "SELECT id,conversation_id,message_id,provider_profile_id,request_id,status,"
            "error_code,http_status,first_delta_ms,total_ms,input_tokens,output_tokens,created_at "
            "FROM request_attempts WHERE conversation_id=? ORDER BY created_at,id"));
        query.addBindValue(conversationId);
    }
    if (!query.exec()) {
        setLastError(QStringLiteral("Read request attempts"), query.lastError());
        return result;
    }
    while (query.next()) {
        result.append(QVariantMap{
            {QStringLiteral("id"), query.value(0)},
            {QStringLiteral("conversationId"), query.value(1)},
            {QStringLiteral("messageId"), query.value(2)},
            {QStringLiteral("providerProfileId"), query.value(3)},
            {QStringLiteral("requestId"), query.value(4)},
            {QStringLiteral("status"), query.value(5)},
            {QStringLiteral("errorCode"), query.value(6)},
            {QStringLiteral("httpStatus"), query.value(7)},
            {QStringLiteral("firstDeltaMs"), query.value(8)},
            {QStringLiteral("totalMs"), query.value(9)},
            {QStringLiteral("inputTokens"), query.value(10)},
            {QStringLiteral("outputTokens"), query.value(11)},
            {QStringLiteral("createdAt"), query.value(12)}
        });
    }
    m_errorString.clear();
    return result;
}

bool ChatStorage::removeRequestAttempt(const QString &attemptId)
{
    if (!isOpen())
        return fail(QStringLiteral("Remove request attempt: database is not open"));
    if (!safeAttemptIdentifier(attemptId))
        return fail(QStringLiteral("Remove request attempt: invalid ID"));
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("DELETE FROM request_attempts WHERE id=?"));
    query.addBindValue(attemptId);
    if (!query.exec()) {
        setLastError(QStringLiteral("Remove request attempt"), query.lastError());
        return false;
    }
    m_errorString.clear();
    return query.numRowsAffected() > 0;
}

QVariantList ChatStorage::conversations() const
{
    ConversationQuery query;
    query.limit = MaximumHistoryPageSize;
    return queryConversations(query).items;
}

ConversationPage ChatStorage::queryConversations(const ConversationQuery &options) const
{
    ConversationPage page;
    if (!isOpen()) {
        fail(QStringLiteral("Search conversations: database is not open"));
        return page;
    }
    const QString text = options.text.trimmed();
    if (text.size() > MaximumHistoryQueryLength
            || options.model.size() > MaximumHistoryQueryLength
            || options.limit < 1 || options.limit > MaximumHistoryPageSize) {
        fail(QStringLiteral("Search conversations: query limits exceeded"));
        return page;
    }
    const bool hasCursor = options.cursorPinned >= 0
            || !options.cursorUpdatedAt.isEmpty() || !options.cursorId.isEmpty();
    if (hasCursor && (options.cursorPinned < 0 || options.cursorPinned > 1
            || options.cursorUpdatedAt.isEmpty() || options.cursorId.isEmpty())) {
        fail(QStringLiteral("Search conversations: invalid cursor"));
        return page;
    }

    if (options.deletedOnly && options.includeDeleted) {
        fail(QStringLiteral("Search conversations: conflicting deletion filters"));
        return page;
    }
    QString sql = QStringLiteral(
        "SELECT c.id,c.title,c.provider_profile_id,c.model,c.pinned,c.created_at,c.updated_at,c.deleted_at "
        "FROM conversations c WHERE %1")
            .arg(options.includeDeleted ? QStringLiteral("1=1")
                 : options.deletedOnly ? QStringLiteral("c.deleted_at<>''")
                                       : QStringLiteral("c.deleted_at=''"));
    QVariantList bindings;
    if (!text.isEmpty()) {
        sql += QStringLiteral(
            " AND (c.title LIKE ? ESCAPE '\\' OR EXISTS (SELECT 1 FROM messages sm "
            "WHERE sm.conversation_id=c.id AND sm.content LIKE ? ESCAPE '\\'))");
        const QString pattern = QStringLiteral("%") + escapeLike(text) + QStringLiteral("%");
        bindings << pattern << pattern;
    }
    if (!options.model.trimmed().isEmpty()) {
        sql += QStringLiteral(" AND c.model=?");
        bindings << options.model.trimmed();
    }
    if (options.from.isValid()) {
        sql += QStringLiteral(" AND c.updated_at>=?");
        bindings << isoUtc(options.from);
    }
    if (options.to.isValid()) {
        sql += QStringLiteral(" AND c.updated_at<=?");
        bindings << isoUtc(options.to);
    }
    if (hasCursor) {
        sql += QStringLiteral(
            " AND (c.pinned<? OR (c.pinned=? AND (c.updated_at<? "
            "OR (c.updated_at=? AND c.id<?))))");
        bindings << options.cursorPinned << options.cursorPinned
                 << options.cursorUpdatedAt << options.cursorUpdatedAt << options.cursorId;
    }
    sql += QStringLiteral(" ORDER BY c.pinned DESC,c.updated_at DESC,c.id DESC LIMIT ?");
    bindings << (options.limit + 1);

    QSqlQuery query(m_database);
    query.prepare(sql);
    for (const QVariant &binding : bindings)
        query.addBindValue(binding);
    if (!query.exec()) {
        setLastError(QStringLiteral("Search conversations"), query.lastError());
        return page;
    }
    while (query.next()) {
        const QVariantMap item{
            {QStringLiteral("id"), query.value(0)}, {QStringLiteral("title"), query.value(1)},
            {QStringLiteral("providerProfileId"), query.value(2)},
            {QStringLiteral("model"), query.value(3)},
            {QStringLiteral("pinned"), query.value(4).toBool()},
            {QStringLiteral("createdAt"), query.value(5)},
            {QStringLiteral("updatedAt"), query.value(6)},
            {QStringLiteral("deletedAt"), query.value(7)}
        };
        if (page.items.size() < options.limit)
            page.items.append(item);
        else {
            page.hasMore = true;
            break;
        }
    }
    if (!page.items.isEmpty()) {
        const QVariantMap last = page.items.last().toMap();
        page.nextCursorPinned = last.value(QStringLiteral("pinned")).toBool() ? 1 : 0;
        page.nextCursorUpdatedAt = last.value(QStringLiteral("updatedAt")).toString();
        page.nextCursorId = last.value(QStringLiteral("id")).toString();
    }
    m_errorString.clear();
    return page;
}

QVariantMap ChatStorage::conversation(const QString &conversationId, bool includeDeleted) const
{
    QVariantMap result;
    if (!isOpen() || conversationId.isEmpty()) {
        fail(QStringLiteral("Read conversation: database is not open or id is empty"));
        return result;
    }
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "SELECT id,title,provider_profile_id,model,pinned,created_at,updated_at,deleted_at "
        "FROM conversations WHERE id=?%1")
        .arg(includeDeleted ? QString() : QStringLiteral(" AND deleted_at=''")));
    query.addBindValue(conversationId);
    if (!query.exec()) {
        setLastError(QStringLiteral("Read conversation"), query.lastError());
        return result;
    }
    if (!query.next()) {
        fail(QStringLiteral("Read conversation: conversation was not found"));
        return result;
    }
    result = {{QStringLiteral("id"), query.value(0)}, {QStringLiteral("title"), query.value(1)},
              {QStringLiteral("providerProfileId"), query.value(2)},
              {QStringLiteral("model"), query.value(3)},
              {QStringLiteral("pinned"), query.value(4).toBool()},
              {QStringLiteral("createdAt"), query.value(5)},
              {QStringLiteral("updatedAt"), query.value(6)},
              {QStringLiteral("deletedAt"), query.value(7)}};
    m_errorString.clear();
    return result;
}

QVariantList ChatStorage::messages(const QString &conversationId) const
{
    QVariantList result;
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "SELECT id,conversation_id,ordinal,role,content,reasoning_content,reference_text,status,"
        "provider_message_id,created_at,updated_at FROM messages "
        "WHERE conversation_id=? ORDER BY ordinal"));
    query.addBindValue(conversationId);
    if (!query.exec()) {
        setLastError(QStringLiteral("List messages"), query.lastError());
        return result;
    }
    while (query.next()) {
        result.append(QVariantMap{
            {QStringLiteral("id"), query.value(0)},
            {QStringLiteral("conversationId"), query.value(1)},
            {QStringLiteral("ordinal"), query.value(2)}, {QStringLiteral("role"), query.value(3)},
            {QStringLiteral("content"), query.value(4)},
            {QStringLiteral("reasoningContent"), query.value(5)},
            {QStringLiteral("reference"), query.value(6)},
            {QStringLiteral("status"), query.value(7)},
            {QStringLiteral("providerMessageId"), query.value(8)},
            {QStringLiteral("createdAt"), query.value(9)},
            {QStringLiteral("updatedAt"), query.value(10)}
        });
    }
    return result;
}

LegacyImportResult ChatStorage::importLegacyChatHistory(const QString &inputPath)
{
    LegacyImportResult result;
    if (!isOpen()) {
        result.errorMessage = QStringLiteral("Database is not open");
        return result;
    }

    const QFileInfo inputInfo(inputPath);
    const QString overallPath = inputInfo.isDir()
        ? QDir(inputInfo.absoluteFilePath()).filePath(QStringLiteral("OverallTable.json"))
        : inputInfo.absoluteFilePath();
    const QFileInfo overallInfo(overallPath);
    const QString sourceKey = overallInfo.canonicalFilePath().isEmpty()
        ? overallInfo.absoluteFilePath() : overallInfo.canonicalFilePath();
    if (legacySourceWasImported(sourceKey)) {
        result.success = true;
        result.alreadyImported = true;
        return result;
    }

    QFile overall(overallPath);
    if (!overall.open(QIODevice::ReadOnly)) {
        result.errorMessage = QStringLiteral("Cannot open legacy history index: %1").arg(overall.errorString());
        return result;
    }
    QJsonParseError parseError;
    const QJsonDocument overallDocument = QJsonDocument::fromJson(overall.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        result.errorMessage = QStringLiteral("Invalid legacy history index JSON: %1").arg(parseError.errorString());
        return result;
    }
    const QVariantList history = historyItems(overallDocument.toVariant());
    if (!m_database.transaction()) {
        result.errorMessage = m_database.lastError().text();
        return result;
    }

    const QString baseDirectory = overallInfo.absolutePath();
    bool ok = true;
    for (const QVariant &entryValue : history) {
        QVariantMap entry = entryValue.toMap();
        QVariantMap conversationObject = entry;
        const QString fileName = entry.value(QStringLiteral("fileName")).toString().trimmed();
        if (!fileName.isEmpty()) {
            QString chatFilePath = QDir(baseDirectory).filePath(fileName);
            if (QFileInfo(chatFilePath).suffix().isEmpty())
                chatFilePath.append(QStringLiteral(".json"));
            if (QFileInfo(chatFilePath).isAbsolute() && !isPathInside(chatFilePath, baseDirectory)) {
                result.errorMessage = QStringLiteral("Legacy chat path escapes ChatHistory directory");
                ok = false;
                break;
            }
            QFile chatFile(chatFilePath);
            if (!chatFile.open(QIODevice::ReadOnly)) {
                result.errorMessage = QStringLiteral("Cannot open legacy chat file %1: %2")
                                          .arg(fileName, chatFile.errorString());
                ok = false;
                break;
            }
            const QJsonDocument chatDocument = QJsonDocument::fromJson(chatFile.readAll(), &parseError);
            if (parseError.error != QJsonParseError::NoError || !chatDocument.isObject()) {
                result.errorMessage = QStringLiteral("Invalid legacy chat JSON: %1").arg(fileName);
                ok = false;
                break;
            }
            conversationObject = chatDocument.object().toVariantMap();
        }

        ConversationRecord conversation;
        conversation.id = uuid();
        conversation.title = entry.value(QStringLiteral("title"),
                                         conversationObject.value(QStringLiteral("title")))
                                 .toString().trimmed();
        if (conversation.title.isEmpty())
            conversation.title = QStringLiteral("Imported chat");
        conversation.createdAt = parseLegacyTime(entry.value(QStringLiteral("modifiedTime")));
        conversation.updatedAt = conversation.createdAt;
        if (!upsertConversation(conversation)) {
            result.errorMessage = m_errorString;
            ok = false;
            break;
        }
        ++result.conversationsImported;

        const QVariantList chats = chatItems(conversationObject);
        for (int ordinal = 0; ordinal < chats.size(); ++ordinal) {
            const QVariantMap legacyMessage = chats.at(ordinal).toMap();
            MessageRecord message;
            message.id = uuid();
            message.conversationId = conversation.id;
            message.ordinal = ordinal;
            message.role = roleFromLegacySender(legacyMessage.value(QStringLiteral("sender")));
            message.content = jsonValueAsText(legacyMessage.value(QStringLiteral("content")));
            message.reasoningContent = jsonValueAsText(
                legacyMessage.value(QStringLiteral("reasoningContent")));
            message.reference = jsonValueAsText(legacyMessage.value(QStringLiteral("reference")));
            message.status = QStringLiteral("complete");
            message.createdAt = conversation.createdAt.addMSecs(ordinal);
            message.updatedAt = message.createdAt;
            if (!appendMessage(message)) {
                result.errorMessage = m_errorString;
                ok = false;
                break;
            }
            ++result.messagesImported;
        }
        if (!ok)
            break;
    }

    if (ok)
        ok = recordLegacyImport(sourceKey, overallInfo, result.conversationsImported,
                                result.messagesImported);
    if (!ok || !m_database.commit()) {
        if (ok)
            result.errorMessage = m_database.lastError().text();
        m_database.rollback();
        result.conversationsImported = 0;
        result.messagesImported = 0;
        return result;
    }
    result.success = true;
    return result;
}

bool ChatStorage::execute(const QString &sql)
{
    QSqlQuery query(m_database);
    if (!query.exec(sql)) {
        setLastError(QStringLiteral("SQL execution"), query.lastError());
        return false;
    }
    m_errorString.clear();
    return true;
}

bool ChatStorage::tableHasColumn(const QString &tableName, const QString &columnName) const
{
    QSqlQuery query(m_database);
    if (!query.exec(QStringLiteral("PRAGMA table_info(%1)").arg(tableName)))
        return false;
    while (query.next()) {
        if (query.value(1).toString() == columnName)
            return true;
    }
    return false;
}

bool ChatStorage::legacySourceWasImported(const QString &sourcePath) const
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "SELECT 1 FROM legacy_imports WHERE source_path=? AND status='complete'"));
    query.addBindValue(sourcePath);
    return query.exec() && query.next();
}

bool ChatStorage::recordLegacyImport(const QString &sourcePath, const QFileInfo &sourceInfo,
                                     int conversationsCount, int messagesCount)
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "INSERT INTO legacy_imports(source_path,source_size,source_modified_at,imported_at,"
        "conversations_imported,messages_imported,status) VALUES(?,?,?,?,?,?,?)"));
    query.addBindValue(sourcePath);
    query.addBindValue(sourceInfo.size());
    query.addBindValue(isoUtc(sourceInfo.lastModified()));
    query.addBindValue(isoUtc(QDateTime::currentDateTimeUtc()));
    query.addBindValue(conversationsCount);
    query.addBindValue(messagesCount);
    query.addBindValue(QStringLiteral("complete"));
    if (!query.exec()) {
        setLastError(QStringLiteral("Record legacy import"), query.lastError());
        return false;
    }
    return true;
}

int ChatStorage::nextOrdinal(const QString &conversationId) const
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("SELECT COALESCE(MAX(ordinal),-1)+1 FROM messages WHERE conversation_id=?"));
    query.addBindValue(conversationId);
    if (!query.exec() || !query.next()) {
        setLastError(QStringLiteral("Calculate message ordinal"), query.lastError());
        return -1;
    }
    return query.value(0).toInt();
}

bool ChatStorage::fail(const QString &message) const
{
    m_errorString = message;
    return false;
}

void ChatStorage::setLastError(const QString &prefix, const QSqlError &error) const
{
    m_errorString = QStringLiteral("%1: %2").arg(prefix, error.text());
}

} // namespace smartkey
