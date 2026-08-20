#include "chat_storage.h"
#include "history_exporter.h"
#include "data/data_management_service.h"
#include "diagnostics/diagnostics_exporter.h"
#include "app/chatmodels.h"
#include "app/dialogmanager.h"
#include "ai/aihttptransport.h"
#include "ai/openaicompatibleprovider.h"
#include "settings/provider_settings.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSet>
#include <QTemporaryDir>
#include <QUrl>
#include <QUuid>
#include <QtTest>

using namespace smartkey;

namespace {

class CapturingTransport final : public ai::AiHttpTransport
{
public:
    using AiHttpTransport::AiHttpTransport;

    quint64 start(const ai::AiHttpRequest &request) override
    {
        ++startCount;
        lastRequest = request;
        lastId = static_cast<quint64>(startCount);
        return lastId;
    }

    void cancel(quint64 requestId) override
    {
        ai::AiError error;
        error.code = ai::AiErrorCode::Cancelled;
        error.message = QStringLiteral("transport cancellation detail");
        emit failed(requestId, error);
    }

    void fail(const ai::AiError &error) { emit failed(lastId, error); }

    void completeSse(const QByteArray &payload)
    {
        ai::AiHttpResponseHead head;
        head.statusCode = 200;
        head.headers.insert(QByteArrayLiteral("content-type"),
                            QByteArrayLiteral("text/event-stream"));
        emit responseStarted(static_cast<quint64>(startCount), head);
        emit dataReceived(static_cast<quint64>(startCount), payload);
        emit completed(static_cast<quint64>(startCount));
    }

    void beginSseResponse()
    {
        ai::AiHttpResponseHead head;
        head.statusCode = 200;
        head.headers.insert(QByteArrayLiteral("content-type"),
                            QByteArrayLiteral("text/event-stream"));
        emit responseStarted(lastId, head);
    }

    void pushData(const QByteArray &data) { emit dataReceived(lastId, data); }
    void finishResponse() { emit completed(lastId); }

    int startCount = 0;
    quint64 lastId = 0;
    ai::AiHttpRequest lastRequest;
};

void writeBytes(const QString &path, const QByteArray &bytes)
{
    QFile file(path);
    QVERIFY2(file.open(QIODevice::WriteOnly | QIODevice::Truncate), qPrintable(file.errorString()));
    QCOMPARE(file.write(bytes), qint64(bytes.size()));
}

} // namespace

class ChatStorageTest : public QObject
{
    Q_OBJECT

private slots:
    void schemaCrudAndCrashRecovery();
    void migratesVersionOne();
    void migratesVersionThreeWithoutLosingHistory();
    void importsLegacyHistoryOnce();
    void rejectsEscapingLegacyPathTransactionally();
    void persistTurnRollsBackOnInjectedAssistantFailure();
    void closedAndCorruptStorageReportErrors();
    void dialogManagerFailsClosedWithoutModelMutation();
    void dialogManagerHonorsProviderCapabilities();
    void requestAttemptCrudRejectsSensitiveShapes();
    void dialogManagerRecordsUsageAndAttemptTiming();
    void connectionTestIsMinimalAndDisablesThinking();
    void dialogManagerStopsPendingDeltaTimerOnDestruction();
    void dialogManagerErrorPresentation_data();
    void dialogManagerErrorPresentation();
    void retryDoesNotDuplicateUserAndFailedOutputStaysOutOfContext();
    void requestStateLifecycleAndNeutralCancellation();
    void historySearchIsParameterizedAndBounded();
    void keysetPaginatesTenThousandConversations();
    void recentlyDeletedRestoreAndExplicitExpiryPurge();
    void exportIsScopedAndUsesSafePaths();
    void dialogManagerFiltersAndRestoresHistory();
    void dataManagementPoliciesAndDestructiveActionsAreIsolated();
    void diagnosticsPackageHasZeroCanaryLeakage();
};

void ChatStorageTest::schemaCrudAndCrashRecovery()
{
    QTemporaryDir directory;
    ChatStorage storage(directory.filePath(QStringLiteral("chat.sqlite")));
    QVERIFY2(storage.open(), qPrintable(storage.errorString()));
    QCOMPARE(storage.schemaVersion(), ChatStorage::CurrentSchemaVersion);

    ConversationRecord conversation;
    conversation.id = QStringLiteral("conversation-1");
    conversation.title = QStringLiteral("Test chat");
    conversation.providerProfileId = QStringLiteral("profile-1");
    conversation.model = QStringLiteral("model-1");
    conversation.pinned = true;
    QVERIFY(storage.upsertConversation(conversation));

    MessageRecord message;
    message.id = QStringLiteral("message-1");
    message.conversationId = conversation.id;
    message.role = QStringLiteral("assistant");
    message.content = QStringLiteral("partial");
    message.status = QStringLiteral("streaming");
    QVERIFY2(storage.appendMessage(message), qPrintable(storage.errorString()));
    QVERIFY(storage.markIncompleteMessagesInterrupted());

    const QVariantList conversations = storage.conversations();
    QCOMPARE(conversations.size(), 1);
    QCOMPARE(conversations.first().toMap().value(QStringLiteral("providerProfileId")).toString(),
             QStringLiteral("profile-1"));
    QCOMPARE(conversations.first().toMap().value(QStringLiteral("pinned")).toBool(), true);
    QVERIFY(storage.setConversationTitle(conversation.id, QStringLiteral("Renamed chat")));
    QVERIFY(storage.setConversationPinned(conversation.id, false));
    QCOMPARE(storage.conversations().first().toMap().value(QStringLiteral("title")).toString(),
             QStringLiteral("Renamed chat"));
    QCOMPARE(storage.conversations().first().toMap().value(QStringLiteral("pinned")).toBool(), false);
    const QVariantList messages = storage.messages(conversation.id);
    QCOMPARE(messages.size(), 1);
    QCOMPARE(messages.first().toMap().value(QStringLiteral("status")).toString(),
             QStringLiteral("interrupted"));
    QVERIFY(storage.removeConversation(conversation.id));
    QVERIFY(storage.conversations().isEmpty());
    QCOMPARE(storage.messages(conversation.id).size(), 1);
    QVERIFY(storage.restoreConversation(conversation.id));
    QCOMPARE(storage.conversations().size(), 1);
}

void ChatStorageTest::migratesVersionOne()
{
    QTemporaryDir directory;
    const QString path = directory.filePath(QStringLiteral("version1.sqlite"));
    const QString connection = QUuid::createUuid().toString();
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
        database.setDatabaseName(path);
        QVERIFY(database.open());
        QSqlQuery query(database);
        QVERIFY(query.exec(QStringLiteral(
            "CREATE TABLE conversations(id TEXT PRIMARY KEY,title TEXT NOT NULL,created_at TEXT NOT NULL)")));
        QVERIFY(query.exec(QStringLiteral(
            "CREATE TABLE messages(id TEXT PRIMARY KEY,conversation_id TEXT NOT NULL,ordinal INTEGER NOT NULL,"
            "role TEXT NOT NULL,content TEXT NOT NULL,created_at TEXT NOT NULL)")));
        QVERIFY(query.exec(QStringLiteral("PRAGMA user_version=1")));
        database.close();
    }
    QSqlDatabase::removeDatabase(connection);

    ChatStorage storage(path);
    QVERIFY2(storage.open(), qPrintable(storage.errorString()));
    QCOMPARE(storage.schemaVersion(), ChatStorage::CurrentSchemaVersion);
    ConversationRecord conversation;
    conversation.id = QStringLiteral("new-after-migration");
    conversation.title = QStringLiteral("Migrated schema works");
    conversation.providerProfileId = QStringLiteral("provider");
    QVERIFY2(storage.upsertConversation(conversation), qPrintable(storage.errorString()));
}

void ChatStorageTest::migratesVersionThreeWithoutLosingHistory()
{
    QTemporaryDir directory;
    const QString path = directory.filePath(QStringLiteral("version3.sqlite"));
    const QString connection = QUuid::createUuid().toString();
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
        database.setDatabaseName(path);
        QVERIFY(database.open());
        QSqlQuery query(database);
        QVERIFY(query.exec(QStringLiteral(
            "CREATE TABLE conversations(id TEXT PRIMARY KEY,title TEXT NOT NULL,"
            "provider_profile_id TEXT NOT NULL DEFAULT '',model TEXT NOT NULL DEFAULT '',"
            "pinned INTEGER NOT NULL DEFAULT 0,created_at TEXT NOT NULL,updated_at TEXT NOT NULL)")));
        QVERIFY(query.exec(QStringLiteral(
            "CREATE TABLE messages(id TEXT PRIMARY KEY,conversation_id TEXT NOT NULL,ordinal INTEGER NOT NULL,"
            "role TEXT NOT NULL,content TEXT NOT NULL DEFAULT '',reasoning_content TEXT NOT NULL DEFAULT '',"
            "reference_text TEXT NOT NULL DEFAULT '',status TEXT NOT NULL DEFAULT 'complete',"
            "provider_message_id TEXT NOT NULL DEFAULT '',created_at TEXT NOT NULL,updated_at TEXT NOT NULL,"
            "UNIQUE(conversation_id,ordinal),FOREIGN KEY(conversation_id) REFERENCES conversations(id) ON DELETE CASCADE)")));
        QVERIFY(query.exec(QStringLiteral(
            "INSERT INTO conversations VALUES('kept','Existing history','profile','model',1,"
            "'2026-01-01T00:00:00.000Z','2026-01-02T00:00:00.000Z')")));
        QVERIFY(query.exec(QStringLiteral(
            "INSERT INTO messages VALUES('kept-message','kept',0,'user','must survive','','','complete','',"
            "'2026-01-01T00:00:00.000Z','2026-01-01T00:00:00.000Z')")));
        QVERIFY(query.exec(QStringLiteral("PRAGMA user_version=3")));
        database.close();
    }
    QSqlDatabase::removeDatabase(connection);

    ChatStorage storage(path);
    QVERIFY2(storage.open(), qPrintable(storage.errorString()));
    QCOMPARE(storage.schemaVersion(), 4);
    QCOMPARE(storage.conversations().size(), 1);
    QCOMPARE(storage.conversations().first().toMap().value(QStringLiteral("title")).toString(),
             QStringLiteral("Existing history"));
    QCOMPARE(storage.messages(QStringLiteral("kept")).first().toMap()
             .value(QStringLiteral("content")).toString(), QStringLiteral("must survive"));
}

void ChatStorageTest::importsLegacyHistoryOnce()
{
    QTemporaryDir directory;
    const QString history = directory.filePath(QStringLiteral("ChatHistory"));
    QVERIFY(QDir().mkpath(history));
    writeBytes(QDir(history).filePath(QStringLiteral("OverallTable.json")), R"JSON(
        {"history":[{"title":"旧会话","fileName":"chat-1.json","modifiedTime":"2025-03-01 12:00:00"}]}
    )JSON");
    writeBytes(QDir(history).filePath(QStringLiteral("chat-1.json")), R"JSON(
        {"chat":[
          {"sender":0,"content":"你好","reasoningContent":"","reference":""},
          {"sender":1,"content":"回答","reasoningContent":"推理","reference":"来源"}
        ]}
    )JSON");

    ChatStorage storage(directory.filePath(QStringLiteral("chat.sqlite")));
    QVERIFY(storage.open());
    const LegacyImportResult first = storage.importLegacyChatHistory(history);
    QVERIFY2(first.success, qPrintable(first.errorMessage));
    QVERIFY(!first.alreadyImported);
    QCOMPARE(first.conversationsImported, 1);
    QCOMPARE(first.messagesImported, 2);

    const QVariantList conversations = storage.conversations();
    QCOMPARE(conversations.size(), 1);
    QCOMPARE(conversations.first().toMap().value(QStringLiteral("title")).toString(), QString::fromUtf8("旧会话"));
    const QString id = conversations.first().toMap().value(QStringLiteral("id")).toString();
    const QVariantList messages = storage.messages(id);
    QCOMPARE(messages.at(0).toMap().value(QStringLiteral("role")).toString(), QStringLiteral("user"));
    QCOMPARE(messages.at(1).toMap().value(QStringLiteral("reasoningContent")).toString(), QString::fromUtf8("推理"));
    QCOMPARE(messages.at(1).toMap().value(QStringLiteral("reference")).toString(), QString::fromUtf8("来源"));

    const LegacyImportResult second = storage.importLegacyChatHistory(history);
    QVERIFY(second.success);
    QVERIFY(second.alreadyImported);
    QCOMPARE(storage.conversations().size(), 1);
}

void ChatStorageTest::rejectsEscapingLegacyPathTransactionally()
{
    QTemporaryDir directory;
    const QString history = directory.filePath(QStringLiteral("ChatHistory"));
    QVERIFY(QDir().mkpath(history));
    writeBytes(QDir(history).filePath(QStringLiteral("OverallTable.json")),
               QByteArrayLiteral("{\"history\":[{\"title\":\"bad\",\"fileName\":\"../outside.json\"}]}"));
    writeBytes(directory.filePath(QStringLiteral("outside.json")), QByteArrayLiteral("{\"chat\":[]}"));

    ChatStorage storage(directory.filePath(QStringLiteral("chat.sqlite")));
    QVERIFY(storage.open());
    const LegacyImportResult result = storage.importLegacyChatHistory(history);
    QVERIFY(!result.success);
    QCOMPARE(storage.conversations().size(), 0);
}

void ChatStorageTest::persistTurnRollsBackOnInjectedAssistantFailure()
{
    QTemporaryDir directory;
    const QString path = directory.filePath(QStringLiteral("turn.sqlite"));
    ChatStorage storage(path);
    QVERIFY2(storage.open(), qPrintable(storage.errorString()));

    const QString injectionConnection = QUuid::createUuid().toString();
    {
        QSqlDatabase injection = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                            injectionConnection);
        injection.setDatabaseName(path);
        QVERIFY2(injection.open(), qPrintable(injection.lastError().text()));
        QSqlQuery query(injection);
        QVERIFY2(query.exec(QStringLiteral(
            "CREATE TRIGGER injected_assistant_failure BEFORE INSERT ON messages "
            "WHEN NEW.role='assistant' BEGIN "
            "SELECT RAISE(FAIL, 'injected assistant write failure'); END")),
                 qPrintable(query.lastError().text()));
        injection.close();
    }
    QSqlDatabase::removeDatabase(injectionConnection);

    ConversationRecord conversation;
    conversation.id = QStringLiteral("atomic-turn");
    conversation.title = QStringLiteral("Must roll back");
    MessageRecord user;
    user.id = QStringLiteral("atomic-user");
    user.conversationId = conversation.id;
    user.ordinal = 0;
    user.role = QStringLiteral("user");
    user.content = QStringLiteral("keep this in the input on failure");
    MessageRecord assistant;
    assistant.id = QStringLiteral("atomic-assistant");
    assistant.conversationId = conversation.id;
    assistant.ordinal = 1;
    assistant.role = QStringLiteral("assistant");
    assistant.status = QStringLiteral("in_flight");

    QVERIFY(!storage.persistTurn(conversation, user, assistant));
    QVERIFY2(storage.errorString().contains(QStringLiteral("injected assistant write failure")),
             qPrintable(storage.errorString()));
    QCOMPARE(storage.conversations().size(), 0);
    QCOMPARE(storage.messages(conversation.id).size(), 0);
}

void ChatStorageTest::closedAndCorruptStorageReportErrors()
{
    QTemporaryDir directory;
    ChatStorage closed(directory.filePath(QStringLiteral("closed.sqlite")));
    QVERIFY(closed.open());
    closed.close();
    ConversationRecord conversation;
    conversation.id = QStringLiteral("closed-write");
    conversation.title = QStringLiteral("Should fail");
    QVERIFY(!closed.upsertConversation(conversation));
    QVERIFY(!closed.errorString().isEmpty());

    const QString corruptPath = directory.filePath(QStringLiteral("corrupt.sqlite"));
    writeBytes(corruptPath, QByteArrayLiteral("this is not a sqlite database"));
    ChatStorage corrupt(corruptPath);
    QVERIFY(!corrupt.open());
    QVERIFY(!corrupt.isOpen());
    QVERIFY(!corrupt.errorString().isEmpty());
}

void ChatStorageTest::dialogManagerFailsClosedWithoutModelMutation()
{
    QTemporaryDir directory;
    const QString path = directory.filePath(QStringLiteral("manager.sqlite"));
    ChatStorage storage(path);
    QVERIFY(storage.open());

    const QString injectionConnection = QUuid::createUuid().toString();
    {
        QSqlDatabase injection = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                            injectionConnection);
        injection.setDatabaseName(path);
        QVERIFY(injection.open());
        QSqlQuery query(injection);
        QVERIFY(query.exec(QStringLiteral(
            "CREATE TRIGGER injected_manager_failure BEFORE INSERT ON messages "
            "WHEN NEW.role='assistant' BEGIN SELECT RAISE(FAIL, 'manager injection'); END")));
        injection.close();
    }
    QSqlDatabase::removeDatabase(injectionConnection);

    DialogManager manager(nullptr, nullptr, &storage);
    auto *messages = qobject_cast<ChatConversationModel *>(manager.dialogModel());
    auto *conversations = qobject_cast<ConversationListModel *>(manager.dialogInfoModel());
    QVERIFY(messages);
    QVERIFY(conversations);
    QVERIFY(manager.storageAvailable());
    QVERIFY(!manager.sendMessage(QStringLiteral("input must remain in QML")));
    QVERIFY(manager.storageUnavailable());
    QCOMPARE(manager.temporaryConversation(), false);
    QCOMPARE(manager.lastErrorCode(), QStringLiteral("storage_unavailable"));
    QVERIFY(!manager.storageErrorMessage().isEmpty());
    QCOMPARE(messages->rowCount(), 0);
    QCOMPARE(conversations->rowCount(), 0);
    QCOMPARE(storage.conversations().size(), 0);

    // Once fail-closed, every history mutation reports failure and cannot mutate UI.
    QVERIFY(!manager.deleteChatById(QStringLiteral("missing")));
    QVERIFY(!manager.renameChatById(QStringLiteral("missing"), QStringLiteral("new")));
    QVERIFY(!manager.setChatPinned(QStringLiteral("missing"), true));
    QCOMPARE(conversations->rowCount(), 0);
}

void ChatStorageTest::dialogManagerHonorsProviderCapabilities()
{
    QTemporaryDir directory;
    ProviderSettings settings(directory.filePath(QStringLiteral("settings.ini")),
                              directory.filePath(QStringLiteral("credentials.ini")));
    const QVariantMap values = {
        {QStringLiteral("name"), QStringLiteral("Non-streaming backend schema")},
        {QStringLiteral("baseUrl"), QStringLiteral("https://capability.invalid")},
        {QStringLiteral("chatPath"), QStringLiteral("/v1/chat/completions")},
        {QStringLiteral("model"), QStringLiteral("capability-test")},
        {QStringLiteral("providerType"), QStringLiteral("custom")},
        {QStringLiteral("authScheme"), QStringLiteral("none")},
        {QStringLiteral("capabilitySchema"),
         QStringLiteral("openai-chat-nonstreaming-v1")},
        // Untrusted top-level claims must not become capabilities.
        {QStringLiteral("supportsStreaming"), true},
        {QStringLiteral("supportsSearch"), true},
        {QStringLiteral("searchRequestField"), QStringLiteral("enable_search")}
    };
    const QString id = settings.saveProfile(values);
    QVERIFY(!id.isEmpty());
    QVERIFY(settings.setActiveProfile(id));

    ChatStorage storage(directory.filePath(QStringLiteral("chat.sqlite")));
    QVERIFY(storage.open());
    CapturingTransport transport;
    ai::OpenAiCompatibleProvider provider(&transport);
    DialogManager manager(&settings, &provider, &storage);

    QVERIFY(!manager.searchModeAvailable());
    manager.setSearchModeActive(true);
    QVERIFY(!manager.searchModeActive());
    QVERIFY(manager.sendMessage(QStringLiteral("capability contract")));
    QCOMPARE(transport.startCount, 1);
    const QJsonObject body = QJsonDocument::fromJson(transport.lastRequest.body).object();
    QCOMPARE(body.value(QStringLiteral("stream")).toBool(), false);
    QVERIFY(!body.contains(QStringLiteral("enable_search")));
}

void ChatStorageTest::requestAttemptCrudRejectsSensitiveShapes()
{
    QTemporaryDir directory;
    const QString path = directory.filePath(QStringLiteral("attempts.sqlite"));
    ChatStorage storage(path);
    QVERIFY(storage.open());
    RequestAttemptRecord attempt;
    attempt.id = QStringLiteral("attempt-1");
    attempt.conversationId = QStringLiteral("conversation-1");
    attempt.messageId = QStringLiteral("message-1");
    attempt.providerProfileId = QStringLiteral("profile-1");
    attempt.requestId = QStringLiteral("request-1");
    attempt.status = QStringLiteral("completed");
    attempt.firstDeltaMs = 12;
    attempt.totalMs = 34;
    attempt.inputTokens = 56;
    attempt.outputTokens = 7;
    QVERIFY2(storage.upsertRequestAttempt(attempt), qPrintable(storage.errorString()));
    const QVariantMap stored = storage.requestAttempts().first().toMap();
    QCOMPARE(stored.value(QStringLiteral("inputTokens")).toLongLong(), qint64(56));
    QCOMPARE(stored.value(QStringLiteral("outputTokens")).toLongLong(), qint64(7));
    QVERIFY(!stored.contains(QStringLiteral("content")));
    QVERIFY(!stored.contains(QStringLiteral("apiKey")));
    QVERIFY(!stored.contains(QStringLiteral("url")));

    RequestAttemptRecord rejected = attempt;
    rejected.id = QStringLiteral("attempt-rejected");
    rejected.requestId = QStringLiteral("https://secret.invalid?api_key=canary-secret");
    QVERIFY(!storage.upsertRequestAttempt(rejected));
    QVERIFY(!storage.errorString().contains(QStringLiteral("secret")));
    QCOMPARE(storage.requestAttempts().size(), 1);
    QVERIFY(storage.removeRequestAttempt(attempt.id));
    QVERIFY(storage.requestAttempts().isEmpty());
    QVERIFY(storage.checkpointAndClose());
    QFile database(path);
    QVERIFY(database.open(QIODevice::ReadOnly));
    QVERIFY(!database.readAll().contains("canary-secret"));
}

void ChatStorageTest::dialogManagerRecordsUsageAndAttemptTiming()
{
    QTemporaryDir directory;
    ChatStorage storage(directory.filePath(QStringLiteral("manager-attempt.sqlite")));
    QVERIFY(storage.open());
    ProviderSettings settings(directory.filePath(QStringLiteral("settings.ini")),
                              directory.filePath(QStringLiteral("credentials.ini")));
    const QVariantMap profile{
        {QStringLiteral("name"), QStringLiteral("Local")},
        {QStringLiteral("baseUrl"), QStringLiteral("http://127.0.0.1:12345")},
        {QStringLiteral("chatPath"), QStringLiteral("/v1/chat/completions")},
        {QStringLiteral("model"), QStringLiteral("local-model")},
        {QStringLiteral("providerType"), QStringLiteral("custom")},
        {QStringLiteral("authScheme"), QStringLiteral("none")},
        {QStringLiteral("contextLimit"), 1024},
        {QStringLiteral("outputLimit"), 64}
    };
    const QString profileId = settings.saveProfile(profile);
    QVERIFY(!profileId.isEmpty());
    CapturingTransport transport;
    ai::OpenAiCompatibleProvider provider(&transport);
    DialogManager manager(&settings, &provider, &storage);
    QVERIFY(manager.sendMessage(QStringLiteral("hello")));
    const QJsonObject requestBody = QJsonDocument::fromJson(transport.lastRequest.body).object();
    QCOMPARE(requestBody.value(QStringLiteral("max_tokens")).toInt(), 64);
    QVERIFY(!manager.truncationOccurred());

    transport.beginSseResponse();
    transport.pushData(QByteArrayLiteral(
        "data: {\"choices\":[{\"delta\":{\"content\":\"ok\"}}],"
        "\"usage\":{\"prompt_tokens\":12,\"completion_tokens\":2}}\n\n"
        "data: [DONE]\n\n"));
    transport.finishResponse();
    QCOMPARE(manager.requestState(), QStringLiteral("succeeded"));
    const QVariantMap attempt = storage.requestAttempts().first().toMap();
    QCOMPARE(attempt.value(QStringLiteral("status")).toString(), QStringLiteral("completed"));
    QCOMPARE(attempt.value(QStringLiteral("providerProfileId")).toString(), profileId);
    QCOMPARE(attempt.value(QStringLiteral("inputTokens")).toLongLong(), qint64(12));
    QCOMPARE(attempt.value(QStringLiteral("outputTokens")).toLongLong(), qint64(2));
    QVERIFY(attempt.value(QStringLiteral("firstDeltaMs")).toLongLong() >= 0);
    QVERIFY(attempt.value(QStringLiteral("totalMs")).toLongLong()
            >= attempt.value(QStringLiteral("firstDeltaMs")).toLongLong());
    QVERIFY(!attempt.contains(QStringLiteral("content")));
    QVERIFY(!attempt.contains(QStringLiteral("url")));
}

void ChatStorageTest::connectionTestIsMinimalAndDisablesThinking()
{
    const QVariantMap deepSeek{
        {QStringLiteral("providerType"), QStringLiteral("deepseek")},
        {QStringLiteral("baseUrl"), QStringLiteral("https://api.deepseek.com")},
        {QStringLiteral("model"), QStringLiteral("deepseek-test")},
        {QStringLiteral("thinkingEnabled"), true}
    };
    const ai::AiRequest request = DialogManager::connectionTestRequest(deepSeek);
    QVERIFY(!request.stream);
    QVERIFY(!request.reasoningEnabled);
    QCOMPARE(request.additionalBody.value(QStringLiteral("max_tokens")).toInt(), 4);
    QCOMPARE(request.additionalBody.value(QStringLiteral("thinking")).toMap()
             .value(QStringLiteral("type")).toString(), QStringLiteral("disabled"));
    QCOMPARE(request.messages.size(), 1);
    QCOMPARE(request.messages.first().content, QStringLiteral("OK"));
}

void ChatStorageTest::dialogManagerStopsPendingDeltaTimerOnDestruction()
{
    QTemporaryDir directory;
    ProviderSettings settings(directory.filePath(QStringLiteral("settings.ini")),
                              directory.filePath(QStringLiteral("credentials.ini")));
    const QVariantMap values = {
        {QStringLiteral("name"), QStringLiteral("Streaming backend schema")},
        {QStringLiteral("baseUrl"), QStringLiteral("https://streaming.invalid")},
        {QStringLiteral("chatPath"), QStringLiteral("/v1/chat/completions")},
        {QStringLiteral("model"), QStringLiteral("stream-test")},
        {QStringLiteral("providerType"), QStringLiteral("custom")},
        {QStringLiteral("authScheme"), QStringLiteral("none")}
    };
    const QString id = settings.saveProfile(values);
    QVERIFY(!id.isEmpty());
    QVERIFY(settings.setActiveProfile(id));

    ChatStorage storage(directory.filePath(QStringLiteral("chat.sqlite")));
    QVERIFY(storage.open());
    CapturingTransport transport;
    ai::OpenAiCompatibleProvider provider(&transport);
    auto *manager = new DialogManager(&settings, &provider, &storage);
    QVERIFY(manager->sendMessage(QStringLiteral("leave a delta queued")));
    transport.beginSseResponse();
    transport.pushData(QByteArrayLiteral(
        "data: {\"choices\":[{\"delta\":{\"content\":\"pending\"}}]}\n\n"));

    // Regression: destruction while the 20ms batching timer is active used to
    // crash in QTimer::killTimer. No event-loop pumping is used to hide it.
    delete manager;
}

void ChatStorageTest::dialogManagerErrorPresentation_data()
{
    QTest::addColumn<ai::AiErrorCode>("errorCode");
    QTest::addColumn<QString>("stableCode");
    QTest::addColumn<QString>("action");
    QTest::addColumn<bool>("retryable");
    QTest::addColumn<bool>("neutral");
    QTest::newRow("offline") << ai::AiErrorCode::Offline << QStringLiteral("offline")
                              << QStringLiteral("retry") << true << false;
    QTest::newRow("dns") << ai::AiErrorCode::DnsFailure << QStringLiteral("dns_failure")
                          << QStringLiteral("retry") << true << false;
    QTest::newRow("tls") << ai::AiErrorCode::TlsFailure << QStringLiteral("tls_failure")
                          << QString() << false << false;
    QTest::newRow("proxy") << ai::AiErrorCode::ProxyFailure << QStringLiteral("proxy_failure")
                            << QStringLiteral("retry") << true << false;
    QTest::newRow("401") << ai::AiErrorCode::AuthenticationFailed
                          << QStringLiteral("authentication_failed")
                          << QStringLiteral("settings") << false << false;
    QTest::newRow("403") << ai::AiErrorCode::AuthorizationFailed
                          << QStringLiteral("authorization_failed")
                          << QStringLiteral("settings") << false << false;
    QTest::newRow("404") << ai::AiErrorCode::NotFound << QStringLiteral("not_found")
                          << QStringLiteral("settings") << false << false;
    QTest::newRow("429") << ai::AiErrorCode::RateLimited << QStringLiteral("rate_limited")
                          << QStringLiteral("retry") << true << false;
    QTest::newRow("5xx") << ai::AiErrorCode::HttpServerError
                          << QStringLiteral("http_server_error")
                          << QStringLiteral("retry") << true << false;
    QTest::newRow("timeout") << ai::AiErrorCode::Timeout << QStringLiteral("timeout")
                              << QStringLiteral("retry") << true << false;
    QTest::newRow("protocol") << ai::AiErrorCode::ProtocolError
                               << QStringLiteral("protocol_error") << QString() << false << false;
    QTest::newRow("parse") << ai::AiErrorCode::ParseError << QStringLiteral("parse_error")
                            << QString() << false << false;
    QTest::newRow("cancel") << ai::AiErrorCode::Cancelled << QStringLiteral("cancelled")
                             << QString() << false << true;
}

void ChatStorageTest::dialogManagerErrorPresentation()
{
    QFETCH(ai::AiErrorCode, errorCode);
    QFETCH(QString, stableCode);
    QFETCH(QString, action);
    QFETCH(bool, retryable);
    QFETCH(bool, neutral);
    QTemporaryDir directory;
    ProviderSettings settings(directory.filePath(QStringLiteral("settings.ini")),
                              directory.filePath(QStringLiteral("credentials.ini")));
    const QString profileId = settings.saveProfile({
        {QStringLiteral("name"), QStringLiteral("Error test")},
        {QStringLiteral("baseUrl"), QStringLiteral("https://errors.invalid")},
        {QStringLiteral("chatPath"), QStringLiteral("/v1/chat/completions")},
        {QStringLiteral("model"), QStringLiteral("error-test")},
        {QStringLiteral("providerType"), QStringLiteral("custom")},
        {QStringLiteral("authScheme"), QStringLiteral("none")}
    });
    QVERIFY(!profileId.isEmpty());
    ChatStorage storage(directory.filePath(QStringLiteral("chat.sqlite")));
    QVERIFY(storage.open());
    CapturingTransport transport;
    ai::OpenAiCompatibleProvider provider(&transport);
    DialogManager manager(&settings, &provider, &storage);
    QVERIFY(manager.sendMessage(QStringLiteral("do not duplicate")));

    ai::AiError error;
    error.code = errorCode;
    error.message = QStringLiteral("secret response body https://secret.invalid?key=canary");
    error.retryable = retryable;
    error.retryAfterSeconds = errorCode == ai::AiErrorCode::RateLimited ? 23 : -1;
    error.providerCode = QStringLiteral("unsafe secret/code");
    error.providerType = QStringLiteral("safe_type");
    transport.fail(error);

    QCOMPARE(manager.requestState(), QStringLiteral("failed"));
    QCOMPARE(manager.lastErrorCode(), stableCode);
    QCOMPARE(manager.lastErrorAction(), action);
    QCOMPARE(manager.canRetry(), retryable && !neutral);
    QVERIFY(!manager.lastErrorMessage().contains(QStringLiteral("secret")));
    QVERIFY(manager.lastProviderErrorCode().isEmpty());
    QCOMPARE(manager.lastProviderErrorType(), QStringLiteral("safe_type"));
    QCOMPARE(manager.retryAfterSeconds(), errorCode == ai::AiErrorCode::RateLimited ? 23 : -1);

    auto *model = qobject_cast<ChatConversationModel *>(manager.dialogModel());
    QVERIFY(model);
    QCOMPARE(model->rowCount(), 2);
    const QModelIndex assistant = model->index(1, 0);
    QVERIFY(model->data(assistant, ChatConversationModel::ContentRole).toString().isEmpty());
    QCOMPARE(model->data(assistant, ChatConversationModel::DisplayErrorCodeRole).toString(),
             stableCode);
    QCOMPARE(model->data(assistant, ChatConversationModel::ErrorActionRole).toString(), action);
    QCOMPARE(model->data(assistant, ChatConversationModel::NeutralErrorRole).toBool(), neutral);
    QVERIFY(!model->data(assistant, ChatConversationModel::DisplayErrorMessageRole)
            .toString().contains(QStringLiteral("secret")));
    const QVariantList conversationMessages = storage.messages(
                storage.conversations().first().toMap().value(QStringLiteral("id")).toString());
    QCOMPARE(conversationMessages.size(), 2);
    QVERIFY(conversationMessages.at(1).toMap().value(QStringLiteral("content")).toString().isEmpty());
}

void ChatStorageTest::retryDoesNotDuplicateUserAndFailedOutputStaysOutOfContext()
{
    QTemporaryDir directory;
    ProviderSettings settings(directory.filePath(QStringLiteral("settings.ini")),
                              directory.filePath(QStringLiteral("credentials.ini")));
    const QString profileId = settings.saveProfile({
        {QStringLiteral("name"), QStringLiteral("Retry test")},
        {QStringLiteral("baseUrl"), QStringLiteral("https://retry.invalid")},
        {QStringLiteral("chatPath"), QStringLiteral("/v1/chat/completions")},
        {QStringLiteral("model"), QStringLiteral("retry-test")},
        {QStringLiteral("providerType"), QStringLiteral("custom")},
        {QStringLiteral("authScheme"), QStringLiteral("none")}
    });
    QVERIFY(!profileId.isEmpty());
    ChatStorage storage(directory.filePath(QStringLiteral("chat.sqlite")));
    QVERIFY(storage.open());
    CapturingTransport transport;
    ai::OpenAiCompatibleProvider provider(&transport);
    DialogManager manager(&settings, &provider, &storage);

    QVERIFY(manager.sendMessage(QStringLiteral("first user")));
    QCOMPARE(QJsonDocument::fromJson(transport.lastRequest.body).object()
             .value(QStringLiteral("messages")).toArray().size(), 1);
    ai::AiError offline;
    offline.code = ai::AiErrorCode::Offline;
    offline.retryable = true;
    transport.fail(offline);
    QVERIFY(manager.canRetry());
    QVERIFY(manager.retryLastRequest());
    QCOMPARE(transport.startCount, 2);
    QJsonArray retriedMessages = QJsonDocument::fromJson(transport.lastRequest.body).object()
            .value(QStringLiteral("messages")).toArray();
    QCOMPARE(retriedMessages.size(), 1);
    QCOMPARE(retriedMessages.first().toObject().value(QStringLiteral("content")).toString(),
             QStringLiteral("first user"));

    transport.beginSseResponse();
    transport.pushData(QByteArrayLiteral(
        "data: {\"choices\":[{\"delta\":{\"content\":\"partial secret\"}}]}\n\n"));
    ai::AiError protocol;
    protocol.code = ai::AiErrorCode::ProtocolError;
    protocol.retryable = true;
    transport.fail(protocol);
    QVERIFY(!manager.canRetry());
    auto *model = qobject_cast<ChatConversationModel *>(manager.dialogModel());
    const QModelIndex failedAssistant = model->index(1, 0);
    QCOMPARE(model->data(failedAssistant, ChatConversationModel::ContentRole).toString(),
             QStringLiteral("partial secret"));
    QVERIFY(model->data(failedAssistant, ChatConversationModel::PartialRole).toBool());

    QVERIFY(manager.sendMessage(QStringLiteral("second user")));
    QCOMPARE(transport.startCount, 3);
    const QJsonArray nextContext = QJsonDocument::fromJson(transport.lastRequest.body).object()
            .value(QStringLiteral("messages")).toArray();
    QCOMPARE(nextContext.size(), 2);
    QCOMPARE(nextContext.at(0).toObject().value(QStringLiteral("content")).toString(),
             QStringLiteral("first user"));
    QCOMPARE(nextContext.at(1).toObject().value(QStringLiteral("content")).toString(),
             QStringLiteral("second user"));
    const QString conversationId = storage.conversations().first().toMap()
            .value(QStringLiteral("id")).toString();
    QCOMPARE(storage.messages(conversationId).size(), 4);
}

void ChatStorageTest::requestStateLifecycleAndNeutralCancellation()
{
    QTemporaryDir directory;
    ProviderSettings settings(directory.filePath(QStringLiteral("settings.ini")),
                              directory.filePath(QStringLiteral("credentials.ini")));
    QVERIFY(!settings.saveProfile({
        {QStringLiteral("name"), QStringLiteral("State test")},
        {QStringLiteral("baseUrl"), QStringLiteral("https://state.invalid")},
        {QStringLiteral("chatPath"), QStringLiteral("/v1/chat/completions")},
        {QStringLiteral("model"), QStringLiteral("state-test")},
        {QStringLiteral("providerType"), QStringLiteral("custom")},
        {QStringLiteral("authScheme"), QStringLiteral("none")}
    }).isEmpty());
    ChatStorage storage(directory.filePath(QStringLiteral("chat.sqlite")));
    QVERIFY(storage.open());
    CapturingTransport transport;
    ai::OpenAiCompatibleProvider provider(&transport);
    DialogManager manager(&settings, &provider, &storage);
    QStringList states;
    connect(&manager, &DialogManager::requestStateChanged, &manager,
            [&] { states.append(manager.requestState()); });
    QCOMPARE(manager.requestState(), QStringLiteral("idle"));
    QVERIFY(manager.sendMessage(QStringLiteral("cancel me")));
    QVERIFY(states.contains(QStringLiteral("validating")));
    QVERIFY(states.contains(QStringLiteral("sending")));
    manager.cancelResponse();
    QVERIFY(states.contains(QStringLiteral("cancelling")));
    QCOMPARE(manager.requestState(), QStringLiteral("failed"));
    QCOMPARE(manager.lastErrorCode(), QStringLiteral("cancelled"));
    QVERIFY(!manager.canRetry());
    const auto *model = qobject_cast<ChatConversationModel *>(manager.dialogModel());
    const QModelIndex assistant = model->index(1, 0);
    QCOMPARE(model->data(assistant, ChatConversationModel::StatusRole).toString(),
             QStringLiteral("cancelled"));
    QVERIFY(model->data(assistant, ChatConversationModel::NeutralErrorRole).toBool());
}

void ChatStorageTest::historySearchIsParameterizedAndBounded()
{
    QTemporaryDir directory;
    ChatStorage storage(directory.filePath(QStringLiteral("search.sqlite")));
    QVERIFY(storage.open());
    const QDateTime base = QDateTime::fromString(QStringLiteral("2026-01-01T00:00:00Z"), Qt::ISODate);

    const auto add = [&](const QString &id, const QString &title, const QString &model,
                         const QString &body, int day) {
        ConversationRecord conversation;
        conversation.id = id;
        conversation.title = title;
        conversation.model = model;
        conversation.createdAt = base.addDays(day);
        conversation.updatedAt = conversation.createdAt;
        QVERIFY(storage.upsertConversation(conversation));
        MessageRecord message;
        message.id = id + QStringLiteral("-message");
        message.conversationId = id;
        message.role = QStringLiteral("user");
        message.content = body;
        QVERIFY(storage.appendMessage(message));
    };
    add(QStringLiteral("literal-injection"), QStringLiteral("alpha' OR 1=1 --"),
        QStringLiteral("model-a"), QStringLiteral("ordinary"), 1);
    add(QStringLiteral("body-match"), QStringLiteral("body"), QStringLiteral("model-b"),
        QStringLiteral("needle' OR 1=1 --"), 2);
    add(QStringLiteral("wildcard"), QStringLiteral("100% literal"), QStringLiteral("model-a"),
        QStringLiteral("ordinary"), 3);
    add(QStringLiteral("unrelated"), QStringLiteral("private row"), QStringLiteral("model-a"),
        QStringLiteral("must not leak"), 4);

    ConversationQuery query;
    query.text = QStringLiteral("alpha' OR 1=1 --");
    QCOMPARE(storage.queryConversations(query).items.size(), 1);
    query.text = QStringLiteral("needle' OR 1=1 --");
    QCOMPARE(storage.queryConversations(query).items.first().toMap()
             .value(QStringLiteral("id")).toString(), QStringLiteral("body-match"));
    query.text = QStringLiteral("%");
    QCOMPARE(storage.queryConversations(query).items.size(), 1);

    query.text.clear();
    query.model = QStringLiteral("model-a");
    query.from = base.addDays(3);
    query.to = base.addDays(3);
    const QVariantList filtered = storage.queryConversations(query).items;
    QCOMPARE(filtered.size(), 1);
    QCOMPARE(filtered.first().toMap().value(QStringLiteral("id")).toString(),
             QStringLiteral("wildcard"));

    query = ConversationQuery();
    query.text = QString(201, QLatin1Char('x'));
    QVERIFY(storage.queryConversations(query).items.isEmpty());
    QVERIFY(storage.errorString().contains(QStringLiteral("limits")));
    query.text.clear();
    query.limit = ChatStorage::MaximumHistoryPageSize + 1;
    QVERIFY(storage.queryConversations(query).items.isEmpty());
}

void ChatStorageTest::keysetPaginatesTenThousandConversations()
{
    QTemporaryDir directory;
    ChatStorage storage(directory.filePath(QStringLiteral("pagination.sqlite")));
    QVERIFY(storage.open());
    const QDateTime base = QDateTime::fromString(QStringLiteral("2025-01-01T00:00:00Z"), Qt::ISODate);
    for (int index = 0; index < 10000; ++index) {
        ConversationRecord record;
        record.id = QStringLiteral("conversation-%1").arg(index, 5, 10, QLatin1Char('0'));
        record.title = QStringLiteral("Conversation %1").arg(index);
        record.model = index % 2 ? QStringLiteral("odd") : QStringLiteral("even");
        record.pinned = index < 7;
        record.createdAt = base.addSecs(index);
        record.updatedAt = record.createdAt;
        QVERIFY2(storage.upsertConversation(record), qPrintable(storage.errorString()));
    }

    ConversationQuery query;
    query.limit = ChatStorage::MaximumHistoryPageSize;
    QSet<QString> identifiers;
    int pages = 0;
    for (;;) {
        const ConversationPage page = storage.queryConversations(query);
        QVERIFY2(!page.items.isEmpty(), qPrintable(storage.errorString()));
        ++pages;
        for (const QVariant &value : page.items) {
            const QString id = value.toMap().value(QStringLiteral("id")).toString();
            QVERIFY(!identifiers.contains(id));
            identifiers.insert(id);
        }
        if (!page.hasMore)
            break;
        query.cursorPinned = page.nextCursorPinned;
        query.cursorUpdatedAt = page.nextCursorUpdatedAt;
        query.cursorId = page.nextCursorId;
    }
    QCOMPARE(identifiers.size(), 10000);
    QCOMPARE(pages, 100);
}

void ChatStorageTest::recentlyDeletedRestoreAndExplicitExpiryPurge()
{
    QTemporaryDir directory;
    ChatStorage storage(directory.filePath(QStringLiteral("trash.sqlite")));
    QVERIFY(storage.open());
    for (int index = 0; index < 2; ++index) {
        ConversationRecord record;
        record.id = QStringLiteral("trash-%1").arg(index);
        record.title = record.id;
        QVERIFY(storage.upsertConversation(record));
        MessageRecord message;
        message.id = QStringLiteral("message-%1").arg(index);
        message.conversationId = record.id;
        message.role = QStringLiteral("user");
        message.content = QStringLiteral("recoverable");
        QVERIFY(storage.appendMessage(message));
        RequestAttemptRecord attempt;
        attempt.id = QStringLiteral("trash-attempt-%1").arg(index);
        attempt.conversationId = record.id;
        attempt.status = QStringLiteral("completed");
        QVERIFY(storage.upsertRequestAttempt(attempt));
        QVERIFY(storage.removeConversation(record.id));
    }
    QVERIFY(storage.conversations().isEmpty());
    ConversationQuery deleted;
    deleted.deletedOnly = true;
    QCOMPARE(storage.queryConversations(deleted).items.size(), 2);
    QCOMPARE(storage.requestAttempts().size(), 2);
    QVERIFY(storage.restoreConversation(QStringLiteral("trash-1")));
    QCOMPARE(storage.conversations().size(), 1);
    QCOMPARE(storage.messages(QStringLiteral("trash-1")).size(), 1);
    QCOMPARE(storage.purgeExpiredDeleted(7, QDateTime::currentDateTimeUtc().addDays(8)), 1);
    QVERIFY(storage.queryConversations(deleted).items.isEmpty());
    QCOMPARE(storage.messages(QStringLiteral("trash-0")).size(), 0);
    QCOMPARE(storage.requestAttempts().size(), 1);

    QVERIFY(storage.removeConversation(QStringLiteral("trash-1")));
    QVERIFY(storage.purgeConversation(QStringLiteral("trash-1")));
    QCOMPARE(storage.messages(QStringLiteral("trash-1")).size(), 0);
    QCOMPARE(storage.requestAttempts().size(), 0);
}

void ChatStorageTest::exportIsScopedAndUsesSafePaths()
{
    QTemporaryDir directory;
    ChatStorage storage(directory.filePath(QStringLiteral("export.sqlite")));
    QVERIFY(storage.open());
    ConversationRecord record;
    record.id = QStringLiteral("export-one");
    record.title = QStringLiteral("Export title");
    record.providerProfileId = QStringLiteral("profile-secret-not-exported");
    record.model = QStringLiteral("safe-model");
    QVERIFY(storage.upsertConversation(record));
    MessageRecord message;
    message.id = QStringLiteral("export-message");
    message.conversationId = record.id;
    message.role = QStringLiteral("user");
    message.content = QStringLiteral("Prompt text");
    message.providerMessageId = QStringLiteral("provider-message-secret");
    QVERIFY(storage.appendMessage(message));

    QString path;
    QString error;
    QVERIFY2(HistoryExporter::exportConversation(
        storage, record.id, directory.filePath(QStringLiteral("exports")),
        QStringLiteral("../CON:<unsafe>"), HistoryExporter::Format::Json, &path, &error),
        qPrintable(error));
    QCOMPARE(QFileInfo(path).absolutePath(),
             QFileInfo(directory.filePath(QStringLiteral("exports"))).absoluteFilePath());
    QVERIFY(!QFileInfo(path).fileName().contains(QLatin1Char('/')));
    QFile json(path);
    QVERIFY(json.open(QIODevice::ReadOnly));
    const QByteArray jsonBytes = json.readAll();
    QVERIFY(jsonBytes.contains("Prompt text"));
    QVERIFY(!jsonBytes.contains("profile-secret-not-exported"));
    QVERIFY(!jsonBytes.contains("provider-message-secret"));
    QVERIFY(!jsonBytes.contains("apiKey"));
    QVERIFY(!jsonBytes.contains("requestAttempts"));

    ConversationQuery filter;
    filter.model = QStringLiteral("safe-model");
    QVERIFY2(HistoryExporter::exportQuery(
        storage, filter, directory.filePath(QStringLiteral("exports")),
        QStringLiteral("filtered"), HistoryExporter::Format::Markdown, &path, &error),
        qPrintable(error));
    QFile markdown(path);
    QVERIFY(markdown.open(QIODevice::ReadOnly));
    const QByteArray markdownBytes = markdown.readAll();
    QVERIFY(markdownBytes.contains("# Export title"));
    QVERIFY(markdownBytes.contains("Prompt text"));
}

void ChatStorageTest::dialogManagerFiltersAndRestoresHistory()
{
    QTemporaryDir directory;
    ProviderSettings settings(directory.filePath(QStringLiteral("settings.ini")),
                              directory.filePath(QStringLiteral("credentials.ini")));
    ChatStorage storage(directory.filePath(QStringLiteral("manager-history.sqlite")));
    QVERIFY(storage.open());
    for (int index = 0; index < 2; ++index) {
        ConversationRecord record;
        record.id = QStringLiteral("manager-history-%1").arg(index);
        record.title = index == 0 ? QStringLiteral("First title") : QStringLiteral("Second title");
        record.model = index == 0 ? QStringLiteral("model-a") : QStringLiteral("model-b");
        record.updatedAt = QDateTime::currentDateTimeUtc().addSecs(index);
        QVERIFY(storage.upsertConversation(record));
        MessageRecord message;
        message.id = QStringLiteral("manager-message-%1").arg(index);
        message.conversationId = record.id;
        message.role = QStringLiteral("user");
        message.content = index == 0 ? QStringLiteral("body needle") : QStringLiteral("other");
        QVERIFY(storage.appendMessage(message));
    }
    CapturingTransport transport;
    ai::OpenAiCompatibleProvider provider(&transport);
    DialogManager manager(&settings, &provider, &storage);
    auto *model = qobject_cast<ConversationListModel *>(manager.dialogInfoModel());
    QVERIFY(model);
    QCOMPARE(model->rowCount(), 2);
    QVERIFY(manager.applyHistoryFilter(QStringLiteral("needle"), QStringLiteral("model-a"),
                                       QString(), QString(), false));
    QCOMPARE(model->rowCount(), 1);
    QCOMPARE(model->data(model->index(0, 0), ConversationListModel::IdRole).toString(),
             QStringLiteral("manager-history-0"));
    QVERIFY(!model->data(model->index(0, 0), ConversationListModel::UpdatedAtRole)
            .toString().isEmpty());
    QVERIFY(manager.deleteChatById(QStringLiteral("manager-history-0")));
    QCOMPARE(model->rowCount(), 0);
    QCOMPARE(storage.messages(QStringLiteral("manager-history-0")).size(), 1);

    QVERIFY(manager.applyHistoryFilter(QString(), QString(), QString(), QString(), true));
    QVERIFY(manager.historyDeletedOnly());
    QCOMPARE(model->rowCount(), 1);
    QCOMPARE(model->data(model->index(0, 0), ConversationListModel::HistorySectionRole).toString(),
             QStringLiteral("deleted"));
    QVERIFY(manager.restoreChatById(QStringLiteral("manager-history-0")));
    QCOMPARE(model->rowCount(), 0);
    ConversationQuery active;
    active.text = QStringLiteral("needle");
    QCOMPARE(storage.queryConversations(active).items.size(), 1);
}

void ChatStorageTest::dataManagementPoliciesAndDestructiveActionsAreIsolated()
{
    QTemporaryDir directory;
    const QString providerIni = directory.filePath(QStringLiteral("provider.ini"));
    const QString credentialIni = directory.filePath(QStringLiteral("credentials.ini"));
    ProviderSettings settings(providerIni, credentialIni);
    const QString profileId = settings.saveProfileWithCredential({
        {QStringLiteral("name"), QStringLiteral("Data management")},
        {QStringLiteral("baseUrl"), QStringLiteral("https://provider.invalid")},
        {QStringLiteral("chatPath"), QStringLiteral("/v1/chat/completions")},
        {QStringLiteral("model"), QStringLiteral("model")},
        {QStringLiteral("providerType"), QStringLiteral("custom")},
        {QStringLiteral("authScheme"), QStringLiteral("bearer")}
    }, QStringLiteral("sk-isolated-test-credential"));
    QVERIFY(!profileId.isEmpty());
    QVERIFY(settings.hasCredential(profileId));

    ChatStorage storage(directory.filePath(QStringLiteral("chat.sqlite")));
    QVERIFY(storage.open());
    ConversationRecord old;
    old.id = QStringLiteral("old-chat");
    old.title = QStringLiteral("Old chat");
    old.model = QStringLiteral("model");
    old.createdAt = QDateTime::currentDateTimeUtc().addDays(-20);
    old.updatedAt = old.createdAt;
    QVERIFY(storage.upsertConversation(old));
    MessageRecord message;
    message.id = QStringLiteral("old-message");
    message.conversationId = old.id;
    message.role = QStringLiteral("user");
    message.content = QStringLiteral("exported body");
    QVERIFY(storage.appendMessage(message));
    RequestAttemptRecord attempt;
    attempt.id = QStringLiteral("old-attempt");
    attempt.conversationId = old.id;
    attempt.status = QStringLiteral("completed");
    QVERIFY(storage.upsertRequestAttempt(attempt));

    const QString logPath = directory.filePath(QStringLiteral("logs/app.jsonl"));
    QVERIFY(QDir().mkpath(QFileInfo(logPath).absolutePath()));
    writeBytes(logPath, QByteArrayLiteral("{\"event\":\"started\"}\n"));
    writeBytes(logPath + QStringLiteral(".1"), QByteArrayLiteral("backup\n"));
    DataManagementService service(
        &storage, &settings, directory.filePath(QStringLiteral("data-management.ini")),
        directory.path(), logPath, QStringLiteral("2.0-test"));
    QCOMPARE(service.activeChatCount(), 1);
    QCOMPARE(service.deletedChatCount(), 0);
    QVERIFY(service.databaseBytes() > 0);
    QVERIFY(service.logBytes() > 0);

    QVERIFY(service.setHistoryPersistenceEnabled(false));
    CapturingTransport transport;
    ai::OpenAiCompatibleProvider provider(&transport);
    DialogManager manager(&settings, &provider, &storage);
    QVERIFY(!manager.sendMessage(QStringLiteral("must not be silently dropped")));
    QCOMPARE(manager.lastErrorCode(), QStringLiteral("history_persistence_disabled"));
    QCOMPARE(transport.startCount, 0);
    QCOMPARE(storage.messages(old.id).size(), 1);
    QVERIFY(service.setHistoryPersistenceEnabled(true));

    QVERIFY(service.setRetentionDays(7));
    QVERIFY(service.applyRetentionPolicy());
    QCOMPARE(service.activeChatCount(), 0);
    QCOMPARE(service.deletedChatCount(), 1);

    QString exportedPath;
    QVERIFY(service.exportAllChats(QUrl::fromLocalFile(
        directory.filePath(QStringLiteral("exports"))).toString(), QStringLiteral("json")));
    QVERIFY(service.operationSucceeded());
    const QString marker = QStringLiteral("聊天已导出到 ");
    QVERIFY(service.operationMessage().startsWith(marker));
    exportedPath = service.operationMessage().mid(marker.size());
    QFile exported(exportedPath);
    QVERIFY(exported.open(QIODevice::ReadOnly));
    QVERIFY(exported.readAll().contains("exported body"));

    QVERIFY(service.clearLogs());
    QVERIFY(!QFile::exists(logPath));
    QVERIFY(!QFile::exists(logPath + QStringLiteral(".1")));
    QCOMPARE(service.logBytes(), qint64(0));
    QVERIFY(service.clearAllChats());
    QCOMPARE(service.activeChatCount(), 0);
    QCOMPARE(service.deletedChatCount(), 0);
    QVERIFY(storage.requestAttempts().isEmpty());
    QVERIFY(service.clearAllCredentials());
    QVERIFY(!settings.hasCredential(profileId));
}

void ChatStorageTest::diagnosticsPackageHasZeroCanaryLeakage()
{
    QTemporaryDir directory;
    const QByteArray promptCanary("CANARY_PROMPT_81E9B5");
    const QByteArray keyCanary("sk-CANARYKEY81E9B5");
    const QByteArray urlCanary("https://secret.invalid/private?api_key=CANARYURL81E9B5");
    ChatStorage storage(directory.filePath(QStringLiteral("canary.sqlite")));
    QVERIFY(storage.open());
    ConversationRecord conversation;
    conversation.id = QStringLiteral("canary-chat");
    conversation.title = QString::fromUtf8(promptCanary);
    conversation.providerProfileId = QStringLiteral("provider-secret");
    conversation.model = QStringLiteral("safe-model");
    QVERIFY(storage.upsertConversation(conversation));
    MessageRecord message;
    message.id = QStringLiteral("canary-message");
    message.conversationId = conversation.id;
    message.role = QStringLiteral("user");
    message.content = QString::fromUtf8(promptCanary + keyCanary + urlCanary);
    QVERIFY(storage.appendMessage(message));

    const QString logPath = directory.filePath(QStringLiteral("logs/app.jsonl"));
    QVERIFY(QDir().mkpath(QFileInfo(logPath).absolutePath()));
    const QByteArray log = QByteArrayLiteral(
        "{\"timestamp\":\"2026-01-01T00:00:00Z\",\"level\":\"error\","
        "\"category\":\"application\",\"event\":\"started\",\"fields\":{")
        + QByteArrayLiteral("\"prompt\":\"") + promptCanary
        + QByteArrayLiteral("\",\"apiKey\":\"") + keyCanary
        + QByteArrayLiteral("\",\"url\":\"") + urlCanary
        + QByteArrayLiteral("\"}}\n{\"level\":\"error\",\"category\":\"")
        + promptCanary + QByteArrayLiteral("\",\"event\":\"")
        + keyCanary + QByteArrayLiteral("\"}\n");
    writeBytes(logPath, log);

    QString outputPath;
    QString error;
    QVERIFY2(DiagnosticsExporter::exportPackage(
        storage, logPath, QStringLiteral("2.0-test"),
        directory.filePath(QStringLiteral("diagnostics")), &outputPath, &error),
        qPrintable(error));
    QFile package(outputPath);
    QVERIFY(package.open(QIODevice::ReadOnly));
    const QByteArray bytes = package.readAll();
    QVERIFY(bytes.contains("database_status_and_counts"));
    QVERIFY(bytes.contains("schemaVersion"));
    QVERIFY(bytes.contains("[REDACTED]"));
    QVERIFY(!bytes.contains(promptCanary));
    QVERIFY(!bytes.contains(keyCanary));
    QVERIFY(!bytes.contains(urlCanary));
    QVERIFY(!bytes.contains("https://"));
    QVERIFY(!bytes.contains("provider-secret"));
    QVERIFY(!bytes.contains("canary-message"));
}

QTEST_GUILESS_MAIN(ChatStorageTest)
#include "test_chat_storage.moc"
