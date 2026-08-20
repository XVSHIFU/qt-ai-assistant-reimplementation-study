#include "aihttptransport.h"
#include "legacyrapooprovider.h"
#include "modeldiscoveryservice.h"
#include "openaicompatibleprovider.h"
#include "sseparser.h"
#include "tokenbudget.h"
#include "urlpolicy.h"
#include "winhttptransport.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QtTest>

#include <limits>

using namespace smartkey::ai;

class FakeTransport final : public AiHttpTransport
{
    Q_OBJECT
public:
    explicit FakeTransport(QObject *parent = nullptr) : AiHttpTransport(parent) {}

    quint64 start(const AiHttpRequest &request) override
    {
        lastRequest = request;
        lastId = nextId++;
        return lastId;
    }

    void cancel(quint64 requestId) override
    {
        cancelledIds.append(requestId);
        AiError error;
        error.code = AiErrorCode::Cancelled;
        error.message = QStringLiteral("cancelled");
        emit failed(requestId, error);
    }

    void sendHead(int status, const QByteArray &contentType,
                  const QMap<QByteArray, QByteArray> &extraHeaders = {})
    {
        sendHeadFor(lastId, status, contentType, extraHeaders);
    }

    void sendHeadFor(quint64 requestId, int status, const QByteArray &contentType,
                     const QMap<QByteArray, QByteArray> &extraHeaders = {})
    {
        AiHttpResponseHead head;
        head.statusCode = status;
        if (!contentType.isEmpty())
            head.headers.insert(QByteArrayLiteral("content-type"), contentType);
        for (auto it = extraHeaders.constBegin(); it != extraHeaders.constEnd(); ++it)
            head.headers.insert(it.key(), it.value());
        emit responseStarted(requestId, head);
    }

    void sendChunk(const QByteArray &chunk) { emit dataReceived(lastId, chunk); }
    void sendChunkFor(quint64 requestId, const QByteArray &chunk)
    {
        emit dataReceived(requestId, chunk);
    }
    void finish() { emit completed(lastId); }
    void finishFor(quint64 requestId) { emit completed(requestId); }
    void failFor(quint64 requestId, const AiError &error) { emit failed(requestId, error); }

    AiHttpRequest lastRequest;
    quint64 lastId = 0;
    quint64 nextId = 1;
    QList<quint64> cancelledIds;
};

class AiTests final : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void strictUrlPolicy();
    void incrementalSseParsing();
    void tokenBudgetIsDeterministicAndKeepsWholeTurns();
    void tokenBudgetBoundsTenThousandMessages();
    void requestConstruction();
    void searchFieldRequiresCapabilityAndActiveState();
    void modelDiscoveryMapping();
    void modelDiscoveryLatestGenerationWins();
    void modelDiscoveryCancelSuppressesCallbacks();
    void modelDiscoveryProfileDeletionSuppressesCallbacks();
    void streamingProviderMapping();
    void streamingDoneIsTerminal();
    void streamingFinishReasonIsTerminal();
    void streamingPartialEofFails();
    void streamingEmptyEofFails();
    void streamingConfiguredEofCompletes();
    void streamingCancelAndEofAreExactlyOnce();
    void nonStreamingProviderMapping();
    void httpErrorMapping();
    void httpErrorMapping_data();
    void cancellationMapping();
    void winHttpReadPlanningBoundsAllocations();
    void winHttpReadPlanningRejectsResponseLimit();
    void winHttpLoopbackIsAsynchronous();
    void legacyProviderIsIsolated();
};

void AiTests::initTestCase()
{
    qRegisterMetaType<AiError>();
}

void AiTests::strictUrlPolicy()
{
    QVERIFY(UrlPolicy::validateEndpoint(QUrl(QStringLiteral("https://api.example.test/v1/chat/completions")), false).accepted);
    QVERIFY(!UrlPolicy::validateEndpoint(QUrl(QStringLiteral("http://api.example.test/v1/chat/completions")), true).accepted);
    QVERIFY(!UrlPolicy::validateEndpoint(QUrl(QStringLiteral("http://127.0.0.1:8080/v1/chat/completions")), false).accepted);
    QVERIFY(UrlPolicy::validateEndpoint(QUrl(QStringLiteral("http://127.0.0.1:8080/v1/chat/completions")), true).accepted);
    QVERIFY(UrlPolicy::validateEndpoint(QUrl(QStringLiteral("http://[::1]:8080/v1/chat/completions")), true).accepted);
    QVERIFY(!UrlPolicy::validateEndpoint(QUrl(QStringLiteral("https://user:pass@example.test/api")), false).accepted);
    QVERIFY(!UrlPolicy::validateEndpoint(QUrl(QStringLiteral("https://example.test/api#secret")), false).accepted);
    QVERIFY(!UrlPolicy::validateEndpoint(QUrl(QStringLiteral("file:///tmp/api")), false).accepted);
}

void AiTests::incrementalSseParsing()
{
    const QString path = QFINDTESTDATA("fixtures/openai_stream.sse");
    QVERIFY2(!path.isEmpty(), "SSE fixture was not found");
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray input = file.readAll();

    SseParser parser;
    QList<SseEvent> events;
    for (int offset = 0; offset < input.size(); offset += 7)
        events.append(parser.feed(input.mid(offset, 7)));
    events.append(parser.finish());

    QVERIFY(!parser.hasError());
    QCOMPARE(events.size(), 3);
    QVERIFY(events.at(0).data.contains("reasoning_content"));
    QVERIFY(events.at(1).data.contains("completion_tokens"));
    QCOMPARE(events.at(2).data, QByteArray("[DONE]"));

    SseParser multiline;
    const QList<SseEvent> joined = multiline.feed("event: note\r\ndata: first\r\ndata: second\r\n\r\n");
    QCOMPARE(joined.size(), 1);
    QCOMPARE(joined.first().event, QByteArray("note"));
    QCOMPARE(joined.first().data, QByteArray("first\nsecond"));
}

void AiTests::tokenBudgetIsDeterministicAndKeepsWholeTurns()
{
    const TokenBudget budget([](const AiMessage &message) {
        return message.content.toLongLong();
    });
    const QList<AiMessage> messages = {
        {QStringLiteral("system"), QStringLiteral("2")},
        {QStringLiteral("user"), QStringLiteral("3")},
        {QStringLiteral("assistant"), QStringLiteral("3")},
        {QStringLiteral("user"), QStringLiteral("4")},
        {QStringLiteral("assistant"), QStringLiteral("4")}
    };

    const TokenBudgetResult first = budget.fit(messages, 12, 2);
    const TokenBudgetResult second = budget.fit(messages, 12, 2);
    QVERIFY(first.fits);
    QVERIFY(first.truncated);
    QCOMPARE(first.omittedTurns, 1);
    QCOMPARE(first.omittedMessages, 2);
    QCOMPARE(first.estimatedInputTokens, qint64(10));
    QCOMPARE(first.messages.size(), 3);
    QCOMPARE(first.messages.at(0).role, QStringLiteral("system"));
    QCOMPARE(first.messages.at(1).content, QStringLiteral("4"));
    QCOMPARE(first.messages.at(2).content, QStringLiteral("4"));
    QCOMPARE(second.messages.size(), first.messages.size());
    for (int index = 0; index < first.messages.size(); ++index) {
        QCOMPARE(second.messages.at(index).role, first.messages.at(index).role);
        QCOMPARE(second.messages.at(index).content, first.messages.at(index).content);
    }

    const TokenBudgetResult oversizedNewest = budget.fit(
                {{QStringLiteral("system"), QStringLiteral("2")},
                 {QStringLiteral("user"), QStringLiteral("20")}}, 12, 2);
    QVERIFY(!oversizedNewest.fits);
    QVERIFY(oversizedNewest.truncated);
    QCOMPARE(oversizedNewest.messages.size(), 0);
}

void AiTests::tokenBudgetBoundsTenThousandMessages()
{
    QList<AiMessage> messages;
    messages.reserve(10000);
    for (int turn = 0; turn < 5000; ++turn) {
        messages.append({QStringLiteral("user"), QStringLiteral("u%1").arg(turn)});
        messages.append({QStringLiteral("assistant"), QStringLiteral("a%1").arg(turn)});
    }
    const TokenBudget budget([](const AiMessage &) { return qint64(1); });
    const TokenBudgetResult result = budget.fit(messages, 101, 1);

    QVERIFY(result.fits);
    QVERIFY(result.truncated);
    QCOMPARE(result.messages.size(), 100);
    QCOMPARE(result.omittedMessages, 9900);
    QCOMPARE(result.omittedTurns, 4950);
    QCOMPARE(result.messages.first().content, QStringLiteral("u4950"));
    QCOMPARE(result.messages.last().content, QStringLiteral("a4999"));
    QCOMPARE(result.estimatedInputTokens, qint64(100));
}

static AiEndpointConfig validConfig()
{
    AiEndpointConfig config;
    config.endpoint = QUrl(QStringLiteral("https://api.example.test/v1/chat/completions"));
    config.defaultModel = QStringLiteral("model-test");
    config.apiKey = QByteArrayLiteral("unit-test-key");
    config.reasoningRequestField = QByteArrayLiteral("enable_reasoning");
    config.searchRequestField = QByteArrayLiteral("enable_search");
    return config;
}

static AiRequest userRequest(bool stream = true)
{
    AiRequest request;
    request.stream = stream;
    request.reasoningEnabled = true;
    request.searchEnabled = true;
    request.messages.append({QStringLiteral("user"), QStringLiteral("hello")});
    return request;
}

void AiTests::requestConstruction()
{
    AiHttpRequest http;
    const AiError error = OpenAiCompatibleProvider::buildHttpRequest(validConfig(), userRequest(), &http);
    QVERIFY(!error.isError());
    QCOMPARE(http.url, validConfig().endpoint);
    QCOMPARE(http.headers.value("authorization"), QByteArray("Bearer unit-test-key"));
    QVERIFY(!http.headers.value("authorization").isEmpty());

    const QJsonObject body = QJsonDocument::fromJson(http.body).object();
    QCOMPARE(body.value(QStringLiteral("model")).toString(), QStringLiteral("model-test"));
    QVERIFY(body.value(QStringLiteral("stream")).toBool());
    QVERIFY(body.value(QStringLiteral("enable_reasoning")).toBool());
    QVERIFY(body.value(QStringLiteral("enable_search")).toBool());
    QCOMPARE(body.value(QStringLiteral("messages")).toArray().size(), 1);

    AiRequest deepSeekRequest = userRequest(false);
    deepSeekRequest.additionalBody.insert(
                QStringLiteral("thinking"),
                QVariantMap{{QStringLiteral("type"), QStringLiteral("enabled")}});
    deepSeekRequest.additionalBody.insert(QStringLiteral("reasoning_effort"),
                                          QStringLiteral("high"));
    AiHttpRequest deepSeekHttp;
    QVERIFY(!OpenAiCompatibleProvider::buildHttpRequest(
                validConfig(), deepSeekRequest, &deepSeekHttp).isError());
    const QJsonObject deepSeekBody = QJsonDocument::fromJson(deepSeekHttp.body).object();
    QCOMPARE(deepSeekBody.value(QStringLiteral("thinking")).toObject()
             .value(QStringLiteral("type")).toString(), QStringLiteral("enabled"));
    QCOMPARE(deepSeekBody.value(QStringLiteral("reasoning_effort")).toString(),
             QStringLiteral("high"));

    deepSeekRequest.additionalBody.insert(QStringLiteral("model"), QStringLiteral("forbidden"));
    QVERIFY(OpenAiCompatibleProvider::buildHttpRequest(
                validConfig(), deepSeekRequest, &deepSeekHttp).isError());

    AiEndpointConfig invalid = validConfig();
    invalid.endpoint = QUrl(QStringLiteral("http://remote.example.test/api"));
    QVERIFY(OpenAiCompatibleProvider::validateConfig(invalid).isError());
    invalid = validConfig();
    invalid.additionalHeaders.insert(QByteArrayLiteral("Authorization"), QByteArrayLiteral("override"));
    QVERIFY(OpenAiCompatibleProvider::validateConfig(invalid).isError());
}

void AiTests::searchFieldRequiresCapabilityAndActiveState()
{
    AiEndpointConfig withoutSearch = validConfig();
    withoutSearch.searchRequestField.clear();
    AiRequest request = userRequest(false);
    AiHttpRequest http;
    QVERIFY(!OpenAiCompatibleProvider::buildHttpRequest(withoutSearch, request, &http).isError());
    QJsonObject body = QJsonDocument::fromJson(http.body).object();
    QCOMPARE(body.value(QStringLiteral("stream")).toBool(), false);
    QVERIFY(!body.contains(QStringLiteral("enable_search")));

    const AiEndpointConfig withSearch = validConfig();
    request.searchEnabled = false;
    QVERIFY(!OpenAiCompatibleProvider::buildHttpRequest(withSearch, request, &http).isError());
    body = QJsonDocument::fromJson(http.body).object();
    QVERIFY(!body.contains(QStringLiteral("enable_search")));

    request.searchEnabled = true;
    QVERIFY(!OpenAiCompatibleProvider::buildHttpRequest(withSearch, request, &http).isError());
    body = QJsonDocument::fromJson(http.body).object();
    QCOMPARE(body.value(QStringLiteral("enable_search")).toBool(), true);
}

void AiTests::modelDiscoveryMapping()
{
    FakeTransport transport;
    ModelDiscoveryService discovery(&transport);
    QSignalSpy discoveredSpy(&discovery, &ModelDiscoveryService::discovered);
    QSignalSpy failedSpy(&discovery, &ModelDiscoveryService::failed);

    const QVariantMap profile = {
        {QStringLiteral("providerType"), QStringLiteral("deepseek")},
        {QStringLiteral("baseUrl"), QStringLiteral("https://api.deepseek.com")},
        {QStringLiteral("authScheme"), QStringLiteral("bearer")},
        {QStringLiteral("timeoutMs"), 45000}
    };
    discovery.discover(QStringLiteral("profile-1"), profile,
                       QByteArrayLiteral("model-list-key"));
    QCOMPARE(transport.lastRequest.method, QByteArrayLiteral("GET"));
    QCOMPARE(transport.lastRequest.url,
             QUrl(QStringLiteral("https://api.deepseek.com/models")));
    QCOMPARE(transport.lastRequest.headers.value(QByteArrayLiteral("authorization")),
             QByteArrayLiteral("Bearer model-list-key"));
    QVERIFY(transport.lastRequest.body.isEmpty());

    transport.sendHead(200, QByteArrayLiteral("application/json"));
    transport.sendChunk("{\"object\":\"list\",\"data\":[{\"id\":\"deepseek-v4-flash\"},");
    transport.sendChunk("{\"id\":\"deepseek-v4-pro\"}]}");
    transport.finish();
    QCOMPARE(failedSpy.count(), 0);
    QCOMPARE(discoveredSpy.count(), 1);
    QCOMPARE(discoveredSpy.first().at(0).toString(), QStringLiteral("profile-1"));
    QCOMPARE(discoveredSpy.first().at(1).toStringList(),
             QStringList({QStringLiteral("deepseek-v4-flash"),
                          QStringLiteral("deepseek-v4-pro")}));
}

static QVariantMap discoveryProfile(const QString &baseUrl)
{
    return {
        {QStringLiteral("providerType"), QStringLiteral("deepseek")},
        {QStringLiteral("baseUrl"), baseUrl},
        {QStringLiteral("authScheme"), QStringLiteral("bearer")},
        {QStringLiteral("timeoutMs"), 45000}
    };
}

void AiTests::modelDiscoveryLatestGenerationWins()
{
    FakeTransport transport;
    ModelDiscoveryService discovery(&transport);
    QSignalSpy discoveredSpy(&discovery, &ModelDiscoveryService::discovered);
    QSignalSpy failedSpy(&discovery, &ModelDiscoveryService::failed);

    discovery.discover(QStringLiteral("profile-1"),
                       discoveryProfile(QStringLiteral("https://api.deepseek.com/old")),
                       QByteArrayLiteral("old-key"));
    const quint64 oldId = transport.lastId;
    QCOMPARE(transport.lastRequest.url,
             QUrl(QStringLiteral("https://api.deepseek.com/old/models")));

    discovery.discover(QStringLiteral("profile-1"),
                       discoveryProfile(QStringLiteral("https://api.deepseek.com/new")),
                       QByteArrayLiteral("new-key"));
    const quint64 newId = transport.lastId;
    QVERIFY(newId != oldId);
    QVERIFY(transport.cancelledIds.contains(oldId));
    QCOMPARE(transport.lastRequest.url,
             QUrl(QStringLiteral("https://api.deepseek.com/new/models")));
    QCOMPARE(transport.lastRequest.headers.value(QByteArrayLiteral("authorization")),
             QByteArrayLiteral("Bearer new-key"));

    // The latest response commits first.
    transport.sendHeadFor(newId, 200, QByteArrayLiteral("application/json"));
    transport.sendChunkFor(newId,
            QByteArrayLiteral("{\"data\":[{\"id\":\"new-model\"}]}"));
    transport.finishFor(newId);
    QCOMPARE(discoveredSpy.count(), 1);
    QCOMPARE(discoveredSpy.first().at(1).toStringList(),
             QStringList({QStringLiteral("new-model")}));

    // Late success and failure from the superseded endpoint are both ignored.
    transport.sendHeadFor(oldId, 200, QByteArrayLiteral("application/json"));
    transport.sendChunkFor(oldId,
            QByteArrayLiteral("{\"data\":[{\"id\":\"stale-model\"}]}"));
    transport.finishFor(oldId);
    AiError staleError;
    staleError.code = AiErrorCode::NetworkFailure;
    transport.failFor(oldId, staleError);
    QCOMPARE(discoveredSpy.count(), 1);
    QCOMPARE(failedSpy.count(), 0);
}

void AiTests::modelDiscoveryCancelSuppressesCallbacks()
{
    FakeTransport transport;
    ModelDiscoveryService discovery(&transport);
    QSignalSpy discoveredSpy(&discovery, &ModelDiscoveryService::discovered);
    QSignalSpy failedSpy(&discovery, &ModelDiscoveryService::failed);

    discovery.discover(QStringLiteral("profile-1"),
                       discoveryProfile(QStringLiteral("https://api.deepseek.com")),
                       QByteArrayLiteral("key"));
    const quint64 requestId = transport.lastId;
    discovery.cancelProfile(QStringLiteral("profile-1"));
    QVERIFY(transport.cancelledIds.contains(requestId));
    transport.sendHeadFor(requestId, 200, QByteArrayLiteral("application/json"));
    transport.sendChunkFor(requestId,
            QByteArrayLiteral("{\"data\":[{\"id\":\"cancelled-model\"}]}"));
    transport.finishFor(requestId);

    QCOMPARE(discoveredSpy.count(), 0);
    QCOMPARE(failedSpy.count(), 0);
}

void AiTests::modelDiscoveryProfileDeletionSuppressesCallbacks()
{
    FakeTransport transport;
    ModelDiscoveryService discovery(&transport);
    QSignalSpy discoveredSpy(&discovery, &ModelDiscoveryService::discovered);
    QSignalSpy failedSpy(&discovery, &ModelDiscoveryService::failed);

    discovery.discover(QStringLiteral("deleted-profile"),
                       discoveryProfile(QStringLiteral("https://api.deepseek.com")),
                       QByteArrayLiteral("key"));
    const quint64 deletedRequestId = transport.lastId;
    // Profile removal is wired to cancelAll in the application.
    discovery.cancelAll();
    QVERIFY(transport.cancelledIds.contains(deletedRequestId));
    AiError lateError;
    lateError.code = AiErrorCode::AuthenticationFailed;
    transport.failFor(deletedRequestId, lateError);
    transport.finishFor(deletedRequestId);

    QCOMPARE(discoveredSpy.count(), 0);
    QCOMPARE(failedSpy.count(), 0);
}

void AiTests::streamingProviderMapping()
{
    FakeTransport transport;
    OpenAiCompatibleProvider provider(&transport);
    QVERIFY(provider.configure(validConfig()));
    QSignalSpy contentSpy(&provider, &OpenAiCompatibleProvider::contentDelta);
    QSignalSpy reasoningSpy(&provider, &OpenAiCompatibleProvider::reasoningDelta);
    QSignalSpy referenceSpy(&provider, &OpenAiCompatibleProvider::referenceDelta);
    QSignalSpy usageSpy(&provider, &OpenAiCompatibleProvider::usage);
    QSignalSpy completedSpy(&provider, &OpenAiCompatibleProvider::completed);
    QSignalSpy failedSpy(&provider, &OpenAiCompatibleProvider::failed);

    const QString requestId = provider.start(userRequest());
    QVERIFY(!requestId.isEmpty());
    transport.sendHead(200, "text/event-stream; charset=utf-8");

    const QString fixturePath = QFINDTESTDATA("fixtures/openai_stream.sse");
    QFile fixture(fixturePath);
    QVERIFY(fixture.open(QIODevice::ReadOnly));
    const QByteArray stream = fixture.readAll();
    for (int offset = 0; offset < stream.size(); offset += 5)
        transport.sendChunk(stream.mid(offset, 5));
    transport.finish();

    QCOMPARE(failedSpy.count(), 0);
    QCOMPARE(completedSpy.count(), 1);
    QCOMPARE(completedSpy.first().at(0).toString(), requestId);
    QCOMPARE(contentSpy.count(), 2);
    QCOMPARE(reasoningSpy.count(), 1);
    QCOMPARE(referenceSpy.count(), 1);
    QCOMPARE(usageSpy.count(), 1);
    QCOMPARE(contentSpy.at(0).at(1).toString(), QStringLiteral("hello "));
    QCOMPARE(contentSpy.at(1).at(1).toString(), QStringLiteral("world"));
}

void AiTests::streamingDoneIsTerminal()
{
    FakeTransport transport;
    OpenAiCompatibleProvider provider(&transport);
    QVERIFY(provider.configure(validConfig()));
    QSignalSpy completedSpy(&provider, &OpenAiCompatibleProvider::completed);
    QSignalSpy failedSpy(&provider, &OpenAiCompatibleProvider::failed);

    const QString requestId = provider.start(userRequest());
    transport.sendHead(200, "text/event-stream");
    transport.sendChunk("data: [DONE]\n\n");
    transport.finish();

    QCOMPARE(completedSpy.count(), 1);
    QCOMPARE(completedSpy.first().at(0).toString(), requestId);
    QCOMPARE(failedSpy.count(), 0);
}

void AiTests::streamingFinishReasonIsTerminal()
{
    FakeTransport transport;
    OpenAiCompatibleProvider provider(&transport);
    QVERIFY(provider.configure(validConfig()));
    QSignalSpy contentSpy(&provider, &OpenAiCompatibleProvider::contentDelta);
    QSignalSpy completedSpy(&provider, &OpenAiCompatibleProvider::completed);
    QSignalSpy failedSpy(&provider, &OpenAiCompatibleProvider::failed);

    provider.start(userRequest());
    transport.sendHead(200, "text/event-stream");
    transport.sendChunk(
            "data: {\"choices\":[{\"delta\":{\"content\":\"final\"},"
            "\"finish_reason\":\"stop\"}]}\n\n");
    transport.finish();

    QCOMPARE(contentSpy.count(), 1);
    QCOMPARE(contentSpy.first().at(1).toString(), QStringLiteral("final"));
    QCOMPARE(completedSpy.count(), 1);
    QCOMPARE(failedSpy.count(), 0);
}

void AiTests::streamingPartialEofFails()
{
    FakeTransport transport;
    OpenAiCompatibleProvider provider(&transport);
    QVERIFY(provider.configure(validConfig()));
    QSignalSpy contentSpy(&provider, &OpenAiCompatibleProvider::contentDelta);
    QSignalSpy completedSpy(&provider, &OpenAiCompatibleProvider::completed);
    QSignalSpy failedSpy(&provider, &OpenAiCompatibleProvider::failed);

    provider.start(userRequest());
    transport.sendHead(200, "text/event-stream");
    transport.sendChunk(
            "data: {\"choices\":[{\"delta\":{\"content\":\"partial\"}}]}\n\n");
    transport.finish();

    QCOMPARE(contentSpy.count(), 1);
    QCOMPARE(completedSpy.count(), 0);
    QCOMPARE(failedSpy.count(), 1);
    const AiError error = qvariant_cast<AiError>(failedSpy.first().at(1));
    QCOMPARE(error.code, AiErrorCode::ProtocolError);
    QCOMPARE(error.message,
             QStringLiteral("The streaming response ended before a terminal event."));
    QVERIFY(error.retryable);
}

void AiTests::streamingEmptyEofFails()
{
    FakeTransport transport;
    OpenAiCompatibleProvider provider(&transport);
    QVERIFY(provider.configure(validConfig()));
    QSignalSpy completedSpy(&provider, &OpenAiCompatibleProvider::completed);
    QSignalSpy failedSpy(&provider, &OpenAiCompatibleProvider::failed);

    provider.start(userRequest());
    transport.sendHead(200, "text/event-stream");
    transport.finish();

    QCOMPARE(completedSpy.count(), 0);
    QCOMPARE(failedSpy.count(), 1);
    const AiError error = qvariant_cast<AiError>(failedSpy.first().at(1));
    QCOMPARE(error.code, AiErrorCode::ProtocolError);
    QCOMPARE(error.message,
             QStringLiteral("The API returned an empty streaming response."));
}

void AiTests::streamingConfiguredEofCompletes()
{
    FakeTransport transport;
    OpenAiCompatibleProvider provider(&transport);
    AiEndpointConfig config = validConfig();
    config.allowSseEofCompletion = true;
    QVERIFY(provider.configure(config));
    QSignalSpy completedSpy(&provider, &OpenAiCompatibleProvider::completed);
    QSignalSpy failedSpy(&provider, &OpenAiCompatibleProvider::failed);

    provider.start(userRequest());
    transport.sendHead(200, "text/event-stream");
    transport.sendChunk(
            "data: {\"choices\":[{\"delta\":{\"content\":\"legacy\"}}]}\n\n");
    transport.finish();

    QCOMPARE(completedSpy.count(), 1);
    QCOMPARE(failedSpy.count(), 0);
}

void AiTests::streamingCancelAndEofAreExactlyOnce()
{
    FakeTransport transport;
    OpenAiCompatibleProvider provider(&transport);
    QVERIFY(provider.configure(validConfig()));
    QSignalSpy completedSpy(&provider, &OpenAiCompatibleProvider::completed);
    QSignalSpy failedSpy(&provider, &OpenAiCompatibleProvider::failed);

    const QString cancelledId = provider.start(userRequest());
    transport.sendHead(200, "text/event-stream");
    transport.sendChunk(
            "data: {\"choices\":[{\"delta\":{\"content\":\"partial\"}}]}\n\n");
    provider.cancel(cancelledId);
    transport.finish();
    QCOMPARE(completedSpy.count(), 0);
    QCOMPARE(failedSpy.count(), 1);
    QCOMPARE(qvariant_cast<AiError>(failedSpy.first().at(1)).code,
             AiErrorCode::Cancelled);

    // Exercise the opposite ordering: EOF wins, then a late cancel/failure is ignored.
    const QString eofId = provider.start(userRequest());
    transport.sendHead(200, "text/event-stream");
    transport.sendChunk(
            "data: {\"choices\":[{\"delta\":{\"content\":\"partial\"}}]}\n\n");
    transport.finish();
    provider.cancel(eofId);
    transport.cancel(transport.lastId);

    QCOMPARE(completedSpy.count(), 0);
    QCOMPARE(failedSpy.count(), 2);
    QCOMPARE(failedSpy.at(1).at(0).toString(), eofId);
    QCOMPARE(qvariant_cast<AiError>(failedSpy.at(1).at(1)).code,
             AiErrorCode::ProtocolError);
}

void AiTests::nonStreamingProviderMapping()
{
    FakeTransport transport;
    OpenAiCompatibleProvider provider(&transport);
    QVERIFY(provider.configure(validConfig()));
    QSignalSpy contentSpy(&provider, &OpenAiCompatibleProvider::contentDelta);
    QSignalSpy completedSpy(&provider, &OpenAiCompatibleProvider::completed);

    provider.start(userRequest(false));
    transport.sendHead(200, "application/json");
    transport.sendChunk("{\"choices\":[{\"message\":{\"content\":\"complete\",\"reasoning_content\":\"why\"}}],\"usage\":{\"total_tokens\":3}}");
    transport.finish();
    QCOMPARE(contentSpy.count(), 1);
    QCOMPARE(contentSpy.first().at(1).toString(), QStringLiteral("complete"));
    QCOMPARE(completedSpy.count(), 1);
}

void AiTests::httpErrorMapping()
{
    QFETCH(int, status);
    QFETCH(AiErrorCode, expectedCode);
    QFETCH(bool, retryable);
    FakeTransport transport;
    OpenAiCompatibleProvider provider(&transport);
    QVERIFY(provider.configure(validConfig()));
    QSignalSpy failedSpy(&provider, &OpenAiCompatibleProvider::failed);
    provider.start(userRequest());
    transport.sendHead(status, "application/json",
                       {{QByteArrayLiteral("retry-after"), QByteArrayLiteral("17")}});
    transport.sendChunk("{\"error\":{\"message\":\"do not expose this\","
                        "\"code\":\"rate_limit_exceeded\",\"type\":\"requests\"}}");
    transport.finish();
    QCOMPARE(failedSpy.count(), 1);
    const AiError error = qvariant_cast<AiError>(failedSpy.first().at(1));
    QCOMPARE(error.code, expectedCode);
    QCOMPARE(error.httpStatus, status);
    QCOMPARE(error.retryable, retryable);
    QVERIFY(!error.message.contains(QStringLiteral("do not expose")));
    QCOMPARE(error.providerCode, QStringLiteral("rate_limit_exceeded"));
    QCOMPARE(error.providerType, QStringLiteral("requests"));
    QCOMPARE(error.retryAfterSeconds, status == 429 ? 17 : -1);
}

void AiTests::httpErrorMapping_data()
{
    QTest::addColumn<int>("status");
    QTest::addColumn<AiErrorCode>("expectedCode");
    QTest::addColumn<bool>("retryable");
    QTest::newRow("401") << 401 << AiErrorCode::AuthenticationFailed << false;
    QTest::newRow("403") << 403 << AiErrorCode::AuthorizationFailed << false;
    QTest::newRow("404") << 404 << AiErrorCode::NotFound << false;
    QTest::newRow("429") << 429 << AiErrorCode::RateLimited << true;
    QTest::newRow("500") << 500 << AiErrorCode::HttpServerError << true;
    QTest::newRow("503") << 503 << AiErrorCode::HttpServerError << true;
}

void AiTests::cancellationMapping()
{
    FakeTransport transport;
    OpenAiCompatibleProvider provider(&transport);
    QVERIFY(provider.configure(validConfig()));
    QSignalSpy failedSpy(&provider, &OpenAiCompatibleProvider::failed);
    const QString id = provider.start(userRequest());
    provider.cancel(id);
    QCOMPARE(failedSpy.count(), 1);
    QCOMPARE(failedSpy.first().at(0).toString(), id);
    QCOMPARE(qvariant_cast<AiError>(failedSpy.first().at(1)).code, AiErrorCode::Cancelled);
}

void AiTests::winHttpReadPlanningBoundsAllocations()
{
    const quint32 chunkBytes = WinHttpTransport::responseReadChunkBytes();
    const quint64 responseLimit = WinHttpTransport::responseByteLimit();
    QVERIFY(chunkBytes >= 32u * 1024u);
    QVERIFY(chunkBytes <= 64u * 1024u);
    QVERIFY(quint64(chunkBytes) < responseLimit);
    QVERIFY(chunkBytes <= quint32(std::numeric_limits<int>::max()));

    const WinHttpTransport::ResponseReadPlan empty =
            WinHttpTransport::planResponseRead(0, 0);
    QCOMPARE(empty.bytesToRead, quint32(0));
    QVERIFY(!empty.error.isError());

    const WinHttpTransport::ResponseReadPlan small =
            WinHttpTransport::planResponseRead(1024, 0);
    QCOMPARE(small.bytesToRead, quint32(1024));
    QVERIFY(!small.error.isError());

    const WinHttpTransport::ResponseReadPlan chunked =
            WinHttpTransport::planResponseRead(quint64(chunkBytes) * 4u, 0);
    QCOMPARE(chunked.bytesToRead, chunkBytes);
    QVERIFY(!chunked.error.isError());

    // A hostile DWORD must never be converted to int or used as an allocation.
    const WinHttpTransport::ResponseReadPlan hostile =
            WinHttpTransport::planResponseRead(
                quint64(std::numeric_limits<quint32>::max()), 0);
    QCOMPARE(hostile.bytesToRead, quint32(0));
    QCOMPARE(hostile.error.code, AiErrorCode::ProtocolError);
}

void AiTests::winHttpReadPlanningRejectsResponseLimit()
{
    const quint64 responseLimit = WinHttpTransport::responseByteLimit();

    const WinHttpTransport::ResponseReadPlan exactBoundary =
            WinHttpTransport::planResponseRead(8, responseLimit - 8);
    QCOMPARE(exactBoundary.bytesToRead, quint32(8));
    QVERIFY(!exactBoundary.error.isError());

    const WinHttpTransport::ResponseReadPlan exceedsBoundary =
            WinHttpTransport::planResponseRead(9, responseLimit - 8);
    QCOMPARE(exceedsBoundary.bytesToRead, quint32(0));
    QCOMPARE(exceedsBoundary.error.code, AiErrorCode::ProtocolError);
    QCOMPARE(exceedsBoundary.error.message,
             QStringLiteral("The API response exceeded the 64 MiB transport limit."));
    QVERIFY(!exceedsBoundary.error.retryable);

    const WinHttpTransport::ResponseReadPlan alreadyExceeded =
            WinHttpTransport::planResponseRead(1, responseLimit + 1);
    QCOMPARE(alreadyExceeded.bytesToRead, quint32(0));
    QCOMPARE(alreadyExceeded.error.code, AiErrorCode::ProtocolError);
    QCOMPARE(alreadyExceeded.error.message, exceedsBoundary.error.message);
}

void AiTests::winHttpLoopbackIsAsynchronous()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    QByteArray receivedRequest;
    connect(&server, &QTcpServer::newConnection, this, [&] {
        QTcpSocket *socket = server.nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead, socket, [&, socket] {
            receivedRequest.append(socket->readAll());
            const int headerEnd = receivedRequest.indexOf("\r\n\r\n");
            if (headerEnd < 0)
                return;
            int contentLength = 0;
            const QList<QByteArray> headers = receivedRequest.left(headerEnd).split('\n');
            for (QByteArray header : headers) {
                header = header.trimmed();
                if (header.toLower().startsWith("content-length:"))
                    contentLength = header.mid(header.indexOf(':') + 1).trimmed().toInt();
            }
            if (receivedRequest.size() < headerEnd + 4 + contentLength)
                return;
            const QByteArray stream = "data: {\"choices\":[{\"delta\":{\"content\":\"loopback\"}}]}\n\n"
                                      "data: [DONE]\n\n";
            const QByteArray response = "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nContent-Length: "
                    + QByteArray::number(stream.size()) + "\r\nConnection: close\r\n\r\n" + stream;
            socket->write(response);
            socket->disconnectFromHost();
        });
    });

    WinHttpTransport transport;
    OpenAiCompatibleProvider provider(&transport);
    AiEndpointConfig config = validConfig();
    config.endpoint = QUrl(QStringLiteral("http://127.0.0.1:%1/v1/chat/completions")
                           .arg(server.serverPort()));
    config.allowLoopbackHttp = true;
    QVERIFY(provider.configure(config));

    QSignalSpy contentSpy(&provider, &OpenAiCompatibleProvider::contentDelta);
    QSignalSpy completedSpy(&provider, &OpenAiCompatibleProvider::completed);
    QSignalSpy failedSpy(&provider, &OpenAiCompatibleProvider::failed);
    provider.start(userRequest());

    QTRY_COMPARE_WITH_TIMEOUT(completedSpy.count(), 1, 5000);
    QCOMPARE(failedSpy.count(), 0);
    QCOMPARE(contentSpy.count(), 1);
    QCOMPARE(contentSpy.first().at(1).toString(), QStringLiteral("loopback"));
    QVERIFY(receivedRequest.toLower().contains("authorization: bearer unit-test-key"));
    QVERIFY(receivedRequest.contains("\"model\":\"model-test\""));
}

void AiTests::legacyProviderIsIsolated()
{
    LegacyRapooProvider provider;
    QVERIFY(!provider.isEnabled());
    QSignalSpy failedSpy(&provider, &LegacyRapooProvider::failed);
    const QString id = provider.start(userRequest());
    QVERIFY(!id.isEmpty());
    QTRY_COMPARE(failedSpy.count(), 1);
    QCOMPARE(qvariant_cast<AiError>(failedSpy.first().at(1)).code, AiErrorCode::LegacyDisabled);
}

QTEST_MAIN(AiTests)
#include "tst_ai.moc"
