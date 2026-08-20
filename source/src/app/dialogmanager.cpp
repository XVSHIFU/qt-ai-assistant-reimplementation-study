#include "dialogmanager.h"

#include "ai/openaicompatibleprovider.h"
#include "ai/tokenbudget.h"
#include "privacy/privacy_consent_service.h"
#include "settings/credential_store.h"
#include "settings/provider_settings.h"
#include "storage/chat_storage.h"
#include "storage/history_exporter.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QRegularExpression>
#include <QUrl>
#include <QUuid>

using smartkey::ProviderProfile;
using smartkey::ProviderSettings;
using namespace smartkey::ai;

namespace {

QString safeDiagnosticToken(const QString &value)
{
    static const QRegularExpression safeToken(
                QStringLiteral("^[A-Za-z0-9][A-Za-z0-9_.:-]{0,63}$"));
    const QString trimmed = value.trimmed();
    return safeToken.match(trimmed).hasMatch() ? trimmed : QString();
}

QVector<ConversationInfo> conversationInfos(const QVariantList &stored)
{
    QVector<ConversationInfo> conversations;
    conversations.reserve(stored.size());
    for (const QVariant &value : stored) {
        const QVariantMap item = value.toMap();
        conversations.append({item.value(QStringLiteral("id")).toString(),
                              item.value(QStringLiteral("title")).toString(),
                              item.value(QStringLiteral("pinned")).toBool(),
                              item.value(QStringLiteral("model")).toString(),
                              item.value(QStringLiteral("updatedAt")).toString(),
                              item.value(QStringLiteral("deletedAt")).toString()});
    }
    return conversations;
}

QString localDirectoryPath(const QString &pathOrUrl)
{
    const QUrl url(pathOrUrl);
    return url.isLocalFile() ? url.toLocalFile() : pathOrUrl;
}

} // namespace

DialogManager::DialogManager(ProviderSettings *settings,
                             OpenAiCompatibleProvider *provider,
                             smartkey::ChatStorage *storage,
                             QObject *parent)
    : QObject(parent),
      m_settings(settings),
      m_provider(provider),
      m_storage(storage),
      m_dialogModel(this),
      m_dialogInfoModel(this)
{
    m_storageAvailable = m_storage && m_storage->isOpen();
    if (!m_storageAvailable) {
        m_storageErrorMessage = tr("本地历史存储不可用。为避免消息丢失，发送与历史修改已禁用。");
        if (m_storage && !m_storage->errorString().isEmpty())
            m_storageErrorMessage += QStringLiteral(" ") + m_storage->errorString();
    }
    m_deltaTimer.setSingleShot(true);
    m_deltaTimer.setInterval(20);
    connect(&m_deltaTimer, &QTimer::timeout, this, &DialogManager::flushPendingDeltas);
    connect(&m_dialogModel, &ChatConversationModel::appended, this, [this] {
        emit appended();
        emit chatStateChanged();
    });

    if (m_provider) {
        connect(m_provider, &OpenAiCompatibleProvider::contentDelta, this,
                [this](const QString &id, const QString &text) {
            if (id != m_requestId) return;
            noteFirstDelta(id);
            m_pendingContent += text;
            if (!m_deltaTimer.isActive()) m_deltaTimer.start();
        });
        connect(m_provider, &OpenAiCompatibleProvider::reasoningDelta, this,
                [this](const QString &id, const QString &text) {
            if (id != m_requestId) return;
            noteFirstDelta(id);
            m_pendingReasoning += text;
            if (!m_deltaTimer.isActive()) m_deltaTimer.start();
        });
        connect(m_provider, &OpenAiCompatibleProvider::referenceDelta, this,
                [this](const QString &id, const QString &text) {
            if (id != m_requestId) return;
            noteFirstDelta(id);
            m_pendingReference += text;
            if (!m_deltaTimer.isActive()) m_deltaTimer.start();
        });
        connect(m_provider, &OpenAiCompatibleProvider::completed,
                this, &DialogManager::handleCompleted);
        connect(m_provider, &OpenAiCompatibleProvider::failed,
                this, &DialogManager::handleFailed);
        connect(m_provider, &OpenAiCompatibleProvider::usage, this,
                [this](const QString &id, const QVariantMap &usage) {
            if (id != m_requestId || id != m_attemptRequestId)
                return;
            const QVariant input = usage.contains(QStringLiteral("prompt_tokens"))
                    ? usage.value(QStringLiteral("prompt_tokens"))
                    : usage.value(QStringLiteral("input_tokens"));
            const QVariant output = usage.contains(QStringLiteral("completion_tokens"))
                    ? usage.value(QStringLiteral("completion_tokens"))
                    : usage.value(QStringLiteral("output_tokens"));
            bool inputOk = false;
            bool outputOk = false;
            const qint64 inputTokens = input.toLongLong(&inputOk);
            const qint64 outputTokens = output.toLongLong(&outputOk);
            if (inputOk && inputTokens >= 0)
                m_attemptInputTokens = inputTokens;
            if (outputOk && outputTokens >= 0)
                m_attemptOutputTokens = outputTokens;
        });
    }

    if (m_settings) {
        connect(m_settings, &ProviderSettings::testConnectionRequested,
                this, &DialogManager::testConnection);
        const auto syncReasoningDefault = [this] {
            const QVariantMap active = m_settings->profile(m_settings->activeProfileId());
            if (!active.isEmpty())
                setReasonModeActive(active.value(QStringLiteral("thinkingEnabled")).toBool());
            syncProviderCapabilities();
        };
        connect(m_settings, &ProviderSettings::activeProfileIdChanged,
                this, syncReasoningDefault);
        connect(m_settings, &ProviderSettings::profilesChanged,
                this, syncReasoningDefault);
        syncReasoningDefault();
    }

    if (m_storageAvailable) {
        reloadConversationList();
    }
}

DialogManager::~DialogManager()
{
    // A value-member QTimer may still have a queued timeout while tests or the
    // application tear down the manager.  Stop and disconnect it before the
    // QObject base and model members begin destruction.
    m_deltaTimer.stop();
    m_deltaTimer.disconnect(this);
    if (m_provider) {
        QObject::disconnect(m_provider, nullptr, this, nullptr);
        m_provider->cancelAll();
    }
    if (m_testProvider) {
        QObject::disconnect(m_testProvider, nullptr, this, nullptr);
        m_testProvider->cancelAll();
    }
}

void DialogManager::setReasonModeActive(bool value)
{
    if (m_reasonModeActive == value) return;
    m_reasonModeActive = value;
    emit reasonModeActiveChanged();
}

void DialogManager::setSearchModeActive(bool value)
{
    value = value && m_searchModeAvailable;
    if (m_searchModeActive == value) return;
    m_searchModeActive = value;
    emit searchModeActiveChanged();
}

void DialogManager::syncProviderCapabilities()
{
    bool available = false;
    if (m_settings) {
        const QString id = m_settings->activeProfileId();
        const QVariantMap values = m_settings->profile(id);
        if (!values.isEmpty()) {
            const ProviderProfile profile = ProviderProfile::fromVariantMap(values, id);
            available = profile.hasUsableSearchCapability();
        }
    }
    if (m_searchModeAvailable != available) {
        m_searchModeAvailable = available;
        emit searchModeAvailableChanged();
    }
    if (!available)
        setSearchModeActive(false);
}

void DialogManager::setCurrentIndex(int value)
{
    if (value < 0 || value >= m_dialogInfoModel.rowCount() || m_currentIndex == value)
        return;
    // Never swap the backing message model while an asynchronous response still
    // owns a row in it.  The history UI is disabled during generation as well,
    // but keeping this guard here prevents late callbacks corrupting another chat.
    if (!m_responseGenerated)
        return;
    m_currentIndex = value;
    loadCurrentConversation();
    emit currentIndexChanged();
}

void DialogManager::setPrivacyConsentService(smartkey::PrivacyConsentService *service)
{
    m_privacyConsent = service;
}

bool DialogManager::ensurePrivacyConsent(const QString &action)
{
    if (!m_privacyConsent || m_privacyConsent->requireConsent(action))
        return true;
    setError(QStringLiteral("privacy_consent_required"),
             tr("请先阅读并同意当前隐私说明。未向第三方 Provider 发送任何内容。"));
    setRequestState(QStringLiteral("failed"));
    return false;
}

bool DialogManager::ensureStorageAvailable(const QString &operation)
{
    if (m_storageAvailable && m_storage && m_storage->isOpen())
        return true;
    return handleStorageFailure(operation);
}

bool DialogManager::handleStorageFailure(const QString &operation)
{
    const bool changed = m_storageAvailable;
    m_storageAvailable = false;
    const QString detail = m_storage ? m_storage->errorString() : QString();
    m_storageErrorMessage = tr("本地历史存储不可用，%1未执行；已保留当前界面内容。").arg(operation);
    if (!detail.isEmpty())
        m_storageErrorMessage += QStringLiteral(" ") + detail;
    setError(QStringLiteral("storage_unavailable"), m_storageErrorMessage);
    setRequestState(QStringLiteral("failed"));
    if (!m_requestId.isEmpty()) {
        const QString requestId = m_requestId;
        m_requestId.clear();
        m_deltaTimer.stop();
        m_pendingContent.clear();
        m_pendingReasoning.clear();
        m_pendingReference.clear();
        m_streamingRow = -1;
        setResponseGenerated(true);
        if (m_provider)
            m_provider->cancel(requestId);
    }
    if (changed || !m_storageErrorMessage.isEmpty())
        emit storageStateChanged();
    return false;
}

bool DialogManager::failPreparedAssistant(const QString &code, const QString &message)
{
    if (m_streamingRow >= 0 && m_streamingRow < m_messageIds.size()) {
        const ChatMessage *assistant = m_dialogModel.messageAt(m_streamingRow);
        if (!assistant || !ensureStorageAvailable(tr("保存回复状态"))
            || !m_storage->updateMessage(m_messageIds.at(m_streamingRow),
                                         assistant->content, assistant->reasoningContent,
                                         assistant->reference, QStringLiteral("failed"))) {
            return handleStorageFailure(tr("保存回复状态"));
        }
        m_dialogModel.setFailure(m_streamingRow, QStringLiteral("failed"), code,
                                 message, QStringLiteral("settings"), -1,
                                 false, false);
        m_streamingRow = -1;
    }
    setResponseGenerated(true);
    setError(code, message);
    setRequestState(QStringLiteral("failed"));
    return false;
}

smartkey::ConversationRecord DialogManager::currentConversationRecord(
        const QString &id, const QString &title) const
{
    smartkey::ConversationRecord record;
    record.id = id;
    record.title = title;
    if (m_settings) {
        record.providerProfileId = m_settings->activeProfileId();
        record.model = m_settings->profile(record.providerProfileId)
                .value(QStringLiteral("model")).toString();
    }
    return record;
}

bool DialogManager::addNewChat()
{
    if (!m_acceptingRequests || !m_responseGenerated
        || !ensureStorageAvailable(tr("新建对话")))
        return false;
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (!m_storage->upsertConversation(currentConversationRecord(id, tr("新对话"))))
        return handleStorageFailure(tr("新建对话"));
    m_dialogModel.clearAll();
    m_messageIds.clear();
    m_currentIndex = m_dialogInfoModel.append({id, tr("新对话")});
    emit currentIndexChanged();
    emit historyStateChanged();
    emit chatStateChanged();
    return true;
}

bool DialogManager::sendMessage(const QString &message)
{
    const QString trimmed = message.trimmed();
    if (!m_acceptingRequests || trimmed.isEmpty() || !m_responseGenerated)
        return false;
    if (!ensurePrivacyConsent(QStringLiteral("chat")))
        return false;
    if (!ensureStorageAvailable(tr("发送消息")))
        return false;
    if (!m_storage->historyPersistenceEnabled()) {
        setError(QStringLiteral("history_persistence_disabled"),
                 tr("已开启“不保存历史”。为避免静默丢失输入，发送已阻止；请在“数据与隐私”中重新启用本地历史。"));
        setRequestState(QStringLiteral("failed"));
        return false;
    }

    const bool isNewConversation = m_currentIndex < 0
            || m_currentIndex >= m_dialogInfoModel.rowCount();
    const QString conversationId = isNewConversation
            ? QUuid::createUuid().toString(QUuid::WithoutBraces)
            : m_dialogInfoModel.idAt(m_currentIndex);
    QString title = isNewConversation ? tr("新对话")
                                      : m_dialogInfoModel.data(
                                            m_dialogInfoModel.index(m_currentIndex, 0),
                                            ConversationListModel::TitleRole).toString();
    if (m_dialogModel.rowCount() == 0)
        title = trimmed.left(40);

    const QString userMessageId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString assistantMessageId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    smartkey::MessageRecord userRecord;
    userRecord.id = userMessageId;
    userRecord.conversationId = conversationId;
    userRecord.ordinal = m_dialogModel.rowCount();
    userRecord.role = QStringLiteral("user");
    userRecord.content = message;
    userRecord.status = QStringLiteral("complete");
    smartkey::MessageRecord assistantRecord;
    assistantRecord.id = assistantMessageId;
    assistantRecord.conversationId = conversationId;
    assistantRecord.ordinal = userRecord.ordinal + 1;
    assistantRecord.role = QStringLiteral("assistant");
    assistantRecord.status = QStringLiteral("in_flight");
    if (!m_storage->persistTurn(currentConversationRecord(conversationId, title),
                                userRecord, assistantRecord))
        return handleStorageFailure(tr("发送消息"));

    if (isNewConversation) {
        m_currentIndex = m_dialogInfoModel.append({conversationId, title});
        emit currentIndexChanged();
        emit historyStateChanged();
    } else if (m_dialogModel.rowCount() == 0) {
        m_dialogInfoModel.setTitle(m_currentIndex, title);
    }
    m_dialogModel.append({0, message, {}, {}, QStringLiteral("complete")});
    m_messageIds.append(userMessageId);
    m_streamingRow = m_dialogModel.append({1, {}, {}, {}, QStringLiteral("in_flight")});
    m_messageIds.append(assistantMessageId);
    startCurrentConversation();
    return true;
}

bool DialogManager::regenerate()
{
    if (!m_acceptingRequests || !m_responseGenerated || m_dialogModel.rowCount() < 1)
        return false;
    if (!ensurePrivacyConsent(QStringLiteral("chat_regenerate")))
        return false;
    if (!ensureStorageAvailable(tr("重新生成")))
        return false;
    const int last = m_dialogModel.rowCount() - 1;
    const ChatMessage *message = m_dialogModel.messageAt(last);
    if (message && message->sender == 1) {
        if (last >= 0 && last < m_messageIds.size()) {
            if (!m_storage->removeMessage(m_messageIds.at(last)))
                return handleStorageFailure(tr("重新生成"));
            m_messageIds.removeAt(last);
        }
        m_dialogModel.removeAt(last);
    }
    startCurrentConversation();
    return m_storageAvailable;
}

bool DialogManager::retryLastRequest()
{
    if (!m_acceptingRequests || !m_responseGenerated || !m_canRetry
            || !ensurePrivacyConsent(QStringLiteral("chat_retry"))
            || !ensureStorageAvailable(tr("重试请求"))) {
        return false;
    }
    const int row = m_dialogModel.rowCount() - 1;
    const ChatMessage *assistant = m_dialogModel.messageAt(row);
    if (!assistant || assistant->sender != 1 || assistant->status != QLatin1String("failed")
            || assistant->partial || !assistant->content.isEmpty()
            || !assistant->reasoningContent.isEmpty() || !assistant->reference.isEmpty()
            || row >= m_messageIds.size()) {
        return false;
    }
    if (!m_storage->updateMessage(m_messageIds.at(row), QString(), QString(), QString(),
                                  QStringLiteral("in_flight"))) {
        return handleStorageFailure(tr("重试请求"));
    }
    m_streamingRow = row;
    m_dialogModel.clearFailure(row, QStringLiteral("in_flight"));
    startCurrentConversation();
    return !m_responseGenerated;
}

void DialogManager::startCurrentConversation()
{
    if (!m_acceptingRequests)
        return;
    setError(QString(), QString());
    setContextBudgetState(false, QString());
    setRequestState(QStringLiteral("validating"));
    if (!m_settings || !m_provider || !m_settings->configured()) {
        failPreparedAssistant(QStringLiteral("not_configured"),
                              tr("请先在设置中配置可用的 AI Provider。"));
        return;
    }

    QString error;
    if (!configureProvider(m_settings->activeProfileId(), m_provider, &error)) {
        failPreparedAssistant(QStringLiteral("configuration_error"), error);
        return;
    }

    AiRequest request;
    const QVariantMap profile = m_settings->profile(m_settings->activeProfileId());
    const ProviderProfile parsedProfile = ProviderProfile::fromVariantMap(
                profile, m_settings->activeProfileId());
    request.model = parsedProfile.model;
    request.stream = parsedProfile.supportsStreaming;
    request.reasoningEnabled = m_reasonModeActive
            && profile.value(QStringLiteral("supportsReasoning")).toBool();
    request.searchEnabled = m_searchModeActive && m_searchModeAvailable
            && parsedProfile.hasUsableSearchCapability();
    const bool deepSeekProfile = profile.value(QStringLiteral("providerType")).toString()
            == QLatin1String("deepseek")
            || QUrl(profile.value(QStringLiteral("baseUrl")).toString()).host().compare(
                QStringLiteral("api.deepseek.com"), Qt::CaseInsensitive) == 0;
    if (deepSeekProfile) {
        // DeepSeek thinking is enabled by default, so "off" must be sent
        // explicitly rather than represented by an omitted field.
        request.additionalBody.insert(
                    QStringLiteral("thinking"),
                    QVariantMap{{QStringLiteral("type"),
                                 request.reasoningEnabled ? QStringLiteral("enabled")
                                                          : QStringLiteral("disabled")}});
    }
    if (request.reasoningEnabled && profile.value(QStringLiteral("thinkingEnabled")).toBool()) {
        if (!deepSeekProfile)
            request.additionalBody.insert(QStringLiteral("thinking"),
                                          QVariantMap{{QStringLiteral("type"), QStringLiteral("enabled")}});
        request.additionalBody.insert(QStringLiteral("reasoning_effort"),
                                      profile.value(QStringLiteral("reasoningEffort"),
                                                    QStringLiteral("high")));
    }
    request.additionalBody.insert(QStringLiteral("max_tokens"), parsedProfile.outputLimit);
    for (const ChatMessage &message : m_dialogModel.messages()) {
        if (message.sender != 0) {
            // Only successful assistant output becomes model-visible history.
            // Failed, interrupted and cancelled placeholders (including partial
            // text) remain UI-visible but can never contaminate a later turn.
            if (message.status != QLatin1String("complete") || message.content.isEmpty())
                continue;
        }
        request.messages.append({message.sender == 0 ? QStringLiteral("user")
                                                     : QStringLiteral("assistant"),
                                 message.content});
    }
    if (request.messages.isEmpty()) {
        failPreparedAssistant(QStringLiteral("empty_request"), tr("没有可发送的消息。"));
        return;
    }
    const TokenBudgetResult budget = TokenBudget().fit(
                request.messages, parsedProfile.contextLimit, parsedProfile.outputLimit);
    if (!budget.fits) {
        setContextBudgetState(true, tr("最新一轮消息超过当前模型的上下文预算，未发送请求。"));
        failPreparedAssistant(QStringLiteral("context_limit_exceeded"), m_contextSummary);
        return;
    }
    request.messages = budget.messages;
    if (budget.truncated) {
        setContextBudgetState(
                    true,
                    tr("上下文预算已裁剪 %1 个较早完整轮次（%2 条消息）；系统消息与最近完整轮次已保留。")
                        .arg(budget.omittedTurns).arg(budget.omittedMessages));
    } else {
        setContextBudgetState(false, QString());
    }

    if (m_streamingRow < 0) {
        if (!ensureStorageAvailable(tr("生成回复")))
            return;
        const QString assistantMessageId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        smartkey::MessageRecord record;
        record.id = assistantMessageId;
        record.conversationId = m_dialogInfoModel.idAt(m_currentIndex);
        record.ordinal = m_dialogModel.rowCount();
        record.role = QStringLiteral("assistant");
        record.status = QStringLiteral("in_flight");
        if (!m_storage->appendMessage(record)) {
            handleStorageFailure(tr("生成回复"));
            return;
        }
        m_streamingRow = m_dialogModel.append({1, {}, {}, {}, QStringLiteral("in_flight")});
        m_messageIds.append(assistantMessageId);
    }
    m_pendingContent.clear();
    m_pendingReasoning.clear();
    m_pendingReference.clear();
    setResponseGenerated(false);
    setRequestState(QStringLiteral("sending"));
    m_requestId = m_provider->start(request);
    if (m_requestId.isEmpty()) {
        failPreparedAssistant(QStringLiteral("start_failed"), tr("无法启动 AI 请求。"));
    } else if (!beginRequestAttempt(m_requestId, budget.estimatedInputTokens)) {
        handleStorageFailure(tr("记录请求统计"));
    }
}

void DialogManager::cancelResponse()
{
    if (m_responseGenerated || !m_provider || m_requestId.isEmpty())
        return;
    setRequestState(QStringLiteral("cancelling"));
    m_provider->cancel(m_requestId);
}

void DialogManager::stopAcceptingRequests()
{
    m_acceptingRequests = false;
}

void DialogManager::cancelRequestsForShutdown()
{
    if (m_provider && !m_requestId.isEmpty())
        m_provider->cancel(m_requestId);

    if (m_testProvider) {
        OpenAiCompatibleProvider *testProvider = m_testProvider;
        testProvider->cancelAll();
        // A synchronous cancellation callback clears m_testProvider. If the
        // transport reports asynchronously, destruction is the final fallback.
        if (m_testProvider == testProvider) {
            m_testProvider->deleteLater();
            m_testProvider = nullptr;
        }
    }
}

void DialogManager::flushAndPersistShutdownState()
{
    m_deltaTimer.stop();
    if (!m_requestId.isEmpty()) {
        AiError interrupted;
        interrupted.code = AiErrorCode::Cancelled;
        interrupted.message = tr("应用退出时中止了请求。");
        handleFailed(m_requestId, interrupted);
    } else {
        flushPendingDeltas();
    }
    if (m_storage && m_storage->isOpen() && m_currentIndex >= 0)
        persistConversation();
}

void DialogManager::copy(int index)
{
    const ChatMessage *message = m_dialogModel.messageAt(index);
    if (message && QGuiApplication::clipboard())
        QGuiApplication::clipboard()->setText(message->content);
}

bool DialogManager::deleteChatAtIndex(int index)
{
    return deleteChatById(m_dialogInfoModel.idAt(index));
}

bool DialogManager::modifyTitleAtIndex(const QString &title, int index)
{
    return renameChatById(m_dialogInfoModel.idAt(index), title);
}

void DialogManager::selectChatById(const QString &conversationId)
{
    if (m_historyDeletedOnly)
        return;
    setCurrentIndex(m_dialogInfoModel.indexOf(conversationId));
}

bool DialogManager::deleteChatById(const QString &conversationId)
{
    if (!m_responseGenerated || conversationId.isEmpty())
        return false;
    const int index = m_dialogInfoModel.indexOf(conversationId);
    if (index < 0)
        return false;
    if (!ensureStorageAvailable(tr("删除对话")))
        return false;
    const QString previousCurrentId = m_dialogInfoModel.idAt(m_currentIndex);
    if (!m_storage->removeConversation(conversationId))
        return handleStorageFailure(tr("删除对话"));
    reloadConversationList(previousCurrentId == conversationId ? QString() : previousCurrentId);
    emit currentIndexChanged();
    emit historyStateChanged();
    emit chatStateChanged();
    return true;
}

bool DialogManager::renameChatById(const QString &conversationId, const QString &title)
{
    if (!m_responseGenerated)
        return false;
    const int index = m_dialogInfoModel.indexOf(conversationId);
    const QString normalized = title.trimmed().left(40);
    if (index < 0 || normalized.isEmpty())
        return false;
    if (!ensureStorageAvailable(tr("重命名对话")))
        return false;
    if (!m_storage->setConversationTitle(conversationId, normalized))
        return handleStorageFailure(tr("重命名对话"));
    m_dialogInfoModel.setTitle(index, normalized);
    return true;
}

bool DialogManager::setChatPinned(const QString &conversationId, bool pinned)
{
    if (!m_responseGenerated)
        return false;
    const int index = m_dialogInfoModel.indexOf(conversationId);
    if (index < 0 || m_dialogInfoModel.pinnedAt(index) == pinned)
        return false;
    if (!ensureStorageAvailable(tr("置顶对话")))
        return false;
    if (!m_storage->setConversationPinned(conversationId, pinned))
        return handleStorageFailure(tr("置顶对话"));
    reloadConversationList(m_dialogInfoModel.idAt(m_currentIndex));
    emit currentIndexChanged();
    emit historyStateChanged();
    return true;
}

bool DialogManager::applyHistoryFilter(const QString &text, const QString &model,
                                       const QString &fromIso, const QString &toIso,
                                       bool deletedOnly)
{
    if (!ensureStorageAvailable(tr("筛选历史")))
        return false;
    const QString normalizedText = text.trimmed();
    const QString normalizedModel = model.trimmed();
    const QDateTime from = fromIso.trimmed().isEmpty()
            ? QDateTime() : QDateTime::fromString(fromIso, Qt::ISODate);
    const QDateTime to = toIso.trimmed().isEmpty()
            ? QDateTime() : QDateTime::fromString(toIso, Qt::ISODate);
    if (normalizedText.size() > smartkey::ChatStorage::MaximumHistoryQueryLength
            || normalizedModel.size() > smartkey::ChatStorage::MaximumHistoryQueryLength
            || (!fromIso.trimmed().isEmpty() && !from.isValid())
            || (!toIso.trimmed().isEmpty() && !to.isValid())
            || (from.isValid() && to.isValid() && from > to)) {
        m_historyOperationMessage = tr("历史筛选条件无效。");
        emit historyOperationChanged();
        return false;
    }
    const QString preferredId = deletedOnly ? QString()
            : m_dialogInfoModel.idAt(m_currentIndex);
    m_historyFilterText = normalizedText;
    m_historyModelFilter = normalizedModel;
    m_historyFrom = from;
    m_historyTo = to;
    m_historyDeletedOnly = deletedOnly;
    m_historyOperationMessage.clear();
    reloadConversationList(preferredId);
    emit currentIndexChanged();
    emit historyStateChanged();
    emit historyOperationChanged();
    return m_storageAvailable;
}

bool DialogManager::loadMoreHistory()
{
    if (!m_historyHasMore || !ensureStorageAvailable(tr("加载更多历史")))
        return false;
    smartkey::ConversationQuery query;
    query.text = m_historyFilterText;
    query.model = m_historyModelFilter;
    query.from = m_historyFrom;
    query.to = m_historyTo;
    query.deletedOnly = m_historyDeletedOnly;
    query.limit = smartkey::ChatStorage::MaximumHistoryPageSize;
    query.cursorPinned = m_historyCursorPinned;
    query.cursorUpdatedAt = m_historyCursorUpdatedAt;
    query.cursorId = m_historyCursorId;
    const smartkey::ConversationPage page = m_storage->queryConversations(query);
    if (page.items.isEmpty() && !m_storage->errorString().isEmpty())
        return handleStorageFailure(tr("加载更多历史"));
    m_dialogInfoModel.appendAll(conversationInfos(page.items));
    m_historyHasMore = page.hasMore;
    m_historyCursorPinned = page.nextCursorPinned;
    m_historyCursorUpdatedAt = page.nextCursorUpdatedAt;
    m_historyCursorId = page.nextCursorId;
    emit historyStateChanged();
    return true;
}

bool DialogManager::restoreChatById(const QString &conversationId)
{
    if (!m_responseGenerated || !ensureStorageAvailable(tr("恢复对话")))
        return false;
    if (!m_storage->restoreConversation(conversationId))
        return handleStorageFailure(tr("恢复对话"));
    reloadConversationList();
    emit currentIndexChanged();
    emit historyStateChanged();
    return true;
}

bool DialogManager::purgeDeletedChatById(const QString &conversationId)
{
    if (!m_responseGenerated || !ensureStorageAvailable(tr("永久删除对话")))
        return false;
    if (!m_storage->purgeConversation(conversationId))
        return handleStorageFailure(tr("永久删除对话"));
    reloadConversationList();
    emit currentIndexChanged();
    emit historyStateChanged();
    return true;
}

bool DialogManager::purgeExpiredDeletedHistory()
{
    if (!m_responseGenerated || !ensureStorageAvailable(tr("清理过期对话")))
        return false;
    const int removed = m_storage->purgeExpiredDeleted(7);
    if (removed < 0)
        return handleStorageFailure(tr("清理过期对话"));
    reloadConversationList();
    m_historyOperationMessage = tr("已永久清理 %1 个超过 7 天的会话。").arg(removed);
    emit currentIndexChanged();
    emit historyStateChanged();
    emit historyOperationChanged();
    return true;
}

bool DialogManager::exportChatById(const QString &conversationId,
                                   const QString &directoryUrl, const QString &format)
{
    if (!ensureStorageAvailable(tr("导出对话")))
        return false;
    const QVariantMap conversation = m_storage->conversation(conversationId, true);
    if (conversation.isEmpty())
        return false;
    const bool json = format.compare(QStringLiteral("json"), Qt::CaseInsensitive) == 0;
    if (!json && format.compare(QStringLiteral("markdown"), Qt::CaseInsensitive) != 0)
        return false;
    QString path;
    QString error;
    const bool ok = smartkey::HistoryExporter::exportConversation(
        *m_storage, conversationId, localDirectoryPath(directoryUrl),
        conversation.value(QStringLiteral("title")).toString(),
        json ? smartkey::HistoryExporter::Format::Json
             : smartkey::HistoryExporter::Format::Markdown,
        &path, &error);
    m_historyOperationMessage = ok ? tr("已导出到 %1").arg(path) : error;
    emit historyOperationChanged();
    return ok;
}

bool DialogManager::exportFilteredHistory(const QString &directoryUrl, const QString &format)
{
    if (!ensureStorageAvailable(tr("导出历史")))
        return false;
    const bool json = format.compare(QStringLiteral("json"), Qt::CaseInsensitive) == 0;
    if (!json && format.compare(QStringLiteral("markdown"), Qt::CaseInsensitive) != 0)
        return false;
    smartkey::ConversationQuery query;
    query.text = m_historyFilterText;
    query.model = m_historyModelFilter;
    query.from = m_historyFrom;
    query.to = m_historyTo;
    query.deletedOnly = m_historyDeletedOnly;
    QString path;
    QString error;
    const bool ok = smartkey::HistoryExporter::exportQuery(
        *m_storage, query, localDirectoryPath(directoryUrl), tr("聊天历史"),
        json ? smartkey::HistoryExporter::Format::Json
             : smartkey::HistoryExporter::Format::Markdown,
        &path, &error);
    m_historyOperationMessage = ok ? tr("已导出到 %1").arg(path) : error;
    emit historyOperationChanged();
    return ok;
}

void DialogManager::parseJSONToCurrentChat()
{
    loadCurrentConversation();
}

void DialogManager::reloadHistory()
{
    const QString preferred = m_dialogInfoModel.idAt(m_currentIndex);
    reloadConversationList(preferred);
    emit currentIndexChanged();
    emit historyStateChanged();
}

void DialogManager::loadCurrentConversation()
{
    if (!m_storage || !m_storage->isOpen()
            || m_currentIndex < 0 || m_currentIndex >= m_dialogInfoModel.rowCount())
        return;
    QVector<ChatMessage> messages;
    QStringList messageIds;
    const QVariantList stored = m_storage->messages(m_dialogInfoModel.idAt(m_currentIndex));
    messages.reserve(stored.size());
    messageIds.reserve(stored.size());
    for (const QVariant &value : stored) {
        const QVariantMap item = value.toMap();
        const QString role = item.value(QStringLiteral("role")).toString();
        messages.append(ChatMessage(role == QStringLiteral("user") ? 0 : 1,
                                    item.value(QStringLiteral("content")).toString(),
                                    item.value(QStringLiteral("reasoningContent")).toString(),
                                    item.value(QStringLiteral("reference")).toString(),
                                    item.value(QStringLiteral("status")).toString()));
        messageIds.append(item.value(QStringLiteral("id")).toString());
    }
    m_dialogModel.replaceAll(messages);
    m_messageIds = messageIds;
    emit chatStateChanged();
}

void DialogManager::reloadConversationList(const QString &preferredConversationId)
{
    if (!m_storage || !m_storage->isOpen())
        return;
    smartkey::ConversationQuery query;
    query.text = m_historyFilterText;
    query.model = m_historyModelFilter;
    query.from = m_historyFrom;
    query.to = m_historyTo;
    query.deletedOnly = m_historyDeletedOnly;
    query.limit = smartkey::ChatStorage::MaximumHistoryPageSize;
    const smartkey::ConversationPage page = m_storage->queryConversations(query);
    if (page.items.isEmpty() && !m_storage->errorString().isEmpty()) {
        handleStorageFailure(tr("读取历史"));
        return;
    }
    const QVector<ConversationInfo> conversations = conversationInfos(page.items);
    m_dialogInfoModel.replaceAll(conversations);
    m_historyHasMore = page.hasMore;
    m_historyCursorPinned = page.nextCursorPinned;
    m_historyCursorUpdatedAt = page.nextCursorUpdatedAt;
    m_historyCursorId = page.nextCursorId;
    m_currentIndex = m_historyDeletedOnly ? -1 : preferredConversationId.isEmpty()
            ? (conversations.isEmpty() ? -1 : 0)
            : m_dialogInfoModel.indexOf(preferredConversationId);
    if (!m_historyDeletedOnly && m_currentIndex < 0 && !conversations.isEmpty())
        m_currentIndex = 0;
    if (m_currentIndex >= 0)
        loadCurrentConversation();
    else {
        m_dialogModel.clearAll();
        m_messageIds.clear();
        emit chatStateChanged();
    }
}

bool DialogManager::persistConversation()
{
    if (!m_storage || !m_storage->isOpen()
            || m_currentIndex < 0 || m_currentIndex >= m_dialogInfoModel.rowCount())
        return false;
    smartkey::ConversationRecord record;
    record.id = m_dialogInfoModel.idAt(m_currentIndex);
    const QModelIndex itemIndex = m_dialogInfoModel.index(m_currentIndex, 0);
    record.title = m_dialogInfoModel.data(itemIndex, ConversationListModel::TitleRole).toString();
    record.pinned = m_dialogInfoModel.pinnedAt(m_currentIndex);
    if (m_settings) {
        record.providerProfileId = m_settings->activeProfileId();
        record.model = m_settings->profile(record.providerProfileId)
                .value(QStringLiteral("model")).toString();
    }
    if (!m_storage->upsertConversation(record))
        return handleStorageFailure(tr("保存对话"));
    return true;
}

bool DialogManager::flushPendingDeltas()
{
    if (m_streamingRow < 0)
        return true;
    const ChatMessage *message = m_dialogModel.messageAt(m_streamingRow);
    if (!message || m_streamingRow >= m_messageIds.size())
        return handleStorageFailure(tr("保存回复"));
    const QString content = message->content + m_pendingContent;
    const QString reasoning = message->reasoningContent + m_pendingReasoning;
    const QString reference = message->reference + m_pendingReference;
    if (!ensureStorageAvailable(tr("保存回复"))
        || !m_storage->updateMessage(m_messageIds.at(m_streamingRow), content,
                                     reasoning, reference, message->status)) {
        return handleStorageFailure(tr("保存回复"));
    }
    m_dialogModel.appendDelta(m_streamingRow, m_pendingContent,
                              m_pendingReasoning, m_pendingReference);
    m_pendingContent.clear();
    m_pendingReasoning.clear();
    m_pendingReference.clear();
    emit chatStateChanged();
    return true;
}

void DialogManager::handleCompleted(const QString &requestId)
{
    if (requestId != m_requestId)
        return;
    if (!flushPendingDeltas())
        return;
    const ChatMessage *message = m_dialogModel.messageAt(m_streamingRow);
    if (!message || m_streamingRow >= m_messageIds.size()
        || !m_storage->updateMessage(m_messageIds.at(m_streamingRow), message->content,
                                     message->reasoningContent, message->reference,
                                     QStringLiteral("complete"))) {
        handleStorageFailure(tr("完成回复"));
        return;
    }
    if (!updateRequestAttempt(QStringLiteral("completed"))) {
        handleStorageFailure(tr("记录请求统计"));
        return;
    }
    m_dialogModel.setStatus(m_streamingRow, QStringLiteral("complete"));
    m_requestId.clear();
    m_streamingRow = -1;
    setResponseGenerated(true);
    setRequestState(QStringLiteral("succeeded"));
}

void DialogManager::handleFailed(const QString &requestId, const AiError &error)
{
    if (requestId != m_requestId)
        return;
    if (!flushPendingDeltas())
        return;
    const ChatMessage *partial = m_dialogModel.messageAt(m_streamingRow);
    const bool hasPartial = partial && (!partial->content.isEmpty()
            || !partial->reasoningContent.isEmpty() || !partial->reference.isEmpty());
    const QString code = errorCodeName(error.code);
    const QString status = error.code == AiErrorCode::Cancelled
            ? QStringLiteral("cancelled") : QStringLiteral("failed");
    if (!partial || m_streamingRow >= m_messageIds.size()
        || !m_storage->updateMessage(m_messageIds.at(m_streamingRow),
                                     partial->content,
                                     partial->reasoningContent, partial->reference, status)) {
        handleStorageFailure(tr("保存失败状态"));
        return;
    }
    if (!updateRequestAttempt(error.code == AiErrorCode::Cancelled
                              ? QStringLiteral("cancelled") : QStringLiteral("failed"),
                              code, error.httpStatus)) {
        handleStorageFailure(tr("记录请求统计"));
        return;
    }
    setAiError(error, hasPartial);
    m_dialogModel.setFailure(m_streamingRow, status, m_lastErrorCode,
                             m_lastErrorMessage, m_lastErrorAction,
                             m_retryAfterSeconds, hasPartial,
                             error.code == AiErrorCode::Cancelled);
    m_requestId.clear();
    m_streamingRow = -1;
    setResponseGenerated(true);
    setRequestState(QStringLiteral("failed"));
}

void DialogManager::setContextBudgetState(bool truncated, const QString &summary)
{
    if (m_truncationOccurred == truncated && m_contextSummary == summary)
        return;
    m_truncationOccurred = truncated;
    m_contextSummary = summary;
    emit contextBudgetChanged();
}

void DialogManager::noteFirstDelta(const QString &requestId)
{
    if (requestId == m_requestId)
        setRequestState(QStringLiteral("streaming"));
    if (requestId == m_attemptRequestId && m_attemptFirstDeltaMs < 0
            && m_attemptTimer.isValid()) {
        m_attemptFirstDeltaMs = m_attemptTimer.elapsed();
    }
}

bool DialogManager::beginRequestAttempt(const QString &requestId,
                                        qint64 estimatedInputTokens)
{
    if (!m_storage || !m_storage->isOpen() || requestId.isEmpty()
            || m_currentIndex < 0 || m_streamingRow < 0
            || m_streamingRow >= m_messageIds.size()) {
        return false;
    }
    m_attemptId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_attemptRequestId = requestId;
    m_attemptConversationId = m_dialogInfoModel.idAt(m_currentIndex);
    m_attemptMessageId = m_messageIds.at(m_streamingRow);
    m_attemptProfileId = m_settings ? m_settings->activeProfileId() : QString();
    m_attemptFirstDeltaMs = -1;
    m_attemptInputTokens = estimatedInputTokens;
    m_attemptOutputTokens = -1;
    m_attemptCreatedAt = QDateTime::currentDateTimeUtc();
    m_attemptTimer.start();
    return updateRequestAttempt(QStringLiteral("in_flight"));
}

bool DialogManager::updateRequestAttempt(const QString &status,
                                         const QString &errorCode, int httpStatus)
{
    if (!m_storage || !m_storage->isOpen() || m_attemptId.isEmpty())
        return false;
    smartkey::RequestAttemptRecord record;
    record.id = m_attemptId;
    record.conversationId = m_attemptConversationId;
    record.messageId = m_attemptMessageId;
    record.providerProfileId = m_attemptProfileId;
    record.requestId = m_attemptRequestId;
    record.status = status;
    record.errorCode = errorCode;
    record.httpStatus = httpStatus;
    record.firstDeltaMs = m_attemptFirstDeltaMs;
    record.totalMs = status == QLatin1String("in_flight") || !m_attemptTimer.isValid()
            ? -1 : m_attemptTimer.elapsed();
    record.inputTokens = m_attemptInputTokens;
    record.outputTokens = m_attemptOutputTokens;
    record.createdAt = m_attemptCreatedAt;
    const bool saved = m_storage->upsertRequestAttempt(record);
    if (saved && status != QLatin1String("in_flight")) {
        m_attemptId.clear();
        m_attemptRequestId.clear();
        m_attemptConversationId.clear();
        m_attemptMessageId.clear();
        m_attemptProfileId.clear();
        m_attemptTimer.invalidate();
    }
    return saved;
}

bool DialogManager::configureProvider(const QString &profileId,
                                      OpenAiCompatibleProvider *provider,
                                      QString *errorMessage)
{
    const QVariantMap values = m_settings ? m_settings->profile(profileId) : QVariantMap();
    if (values.isEmpty()) {
        if (errorMessage) *errorMessage = tr("找不到所选 Provider 配置。");
        return false;
    }
    const ProviderProfile profile = ProviderProfile::fromVariantMap(values, profileId);
    QString validationError;
    if (!profile.isValid(&validationError)) {
        if (errorMessage) *errorMessage = validationError;
        return false;
    }

    QByteArray credential;
    if (profile.requiresCredential()
            && !m_settings->credential(profileId, &credential, &validationError)) {
        if (errorMessage) *errorMessage = validationError;
        return false;
    }

    QUrl base(profile.baseUrl);
    QString path = profile.chatPath.trimmed();
    if (!path.startsWith(QLatin1Char('/')))
        path.prepend(QLatin1Char('/'));
    base.setPath(path);
    base.setQuery(QString());
    base.setFragment(QString());

    AiEndpointConfig config;
    config.endpoint = base;
    config.defaultModel = profile.model;
    config.timeoutMs = profile.timeoutMs;
    config.allowLoopbackHttp = true;
    config.apiKey = credential;
    if (profile.authScheme.compare(QStringLiteral("None"), Qt::CaseInsensitive) == 0) {
        config.authMode = AiAuthMode::None;
    } else if (profile.authScheme.compare(QStringLiteral("ApiKeyHeader"), Qt::CaseInsensitive) == 0
               || profile.authScheme.compare(QStringLiteral("api-key"), Qt::CaseInsensitive) == 0) {
        config.authMode = AiAuthMode::Header;
        config.apiKeyHeader = profile.authHeaderName.toUtf8();
    } else {
        config.authMode = AiAuthMode::Bearer;
    }
    if (profile.hasUsableSearchCapability())
        config.searchRequestField = profile.searchRequestField.toUtf8();
    AiError aiError;
    const bool ok = provider->configure(config, &aiError);
    smartkey::CredentialStore::clearSensitive(&credential);
    if (!ok && errorMessage)
        *errorMessage = aiError.message;
    return ok;
}

void DialogManager::testConnection(const QString &profileId)
{
    if (!m_acceptingRequests || !m_settings)
        return;
    if (m_testProvider) {
        m_settings->setTestResult(QStringLiteral("failed"), tr("已有连接测试正在运行。"));
        return;
    }
    m_testProvider = new OpenAiCompatibleProvider(nullptr, this);
    QString error;
    if (!configureProvider(profileId, m_testProvider, &error)) {
        m_settings->setTestResult(QStringLiteral("failed"), error);
        m_testProvider->deleteLater();
        m_testProvider = nullptr;
        return;
    }
    m_settings->setTestResult(QStringLiteral("testing"), tr("正在测试连接…"));
    connect(m_testProvider, &OpenAiCompatibleProvider::completed, this,
            [this](const QString &) {
        m_settings->setTestResult(QStringLiteral("succeeded"), tr("连接成功。"));
        m_testProvider->deleteLater();
        m_testProvider = nullptr;
    });
    connect(m_testProvider, &OpenAiCompatibleProvider::failed, this,
            [this](const QString &, const AiError &aiError) {
        m_settings->setTestResult(QStringLiteral("failed"), aiError.message);
        m_testProvider->deleteLater();
        m_testProvider = nullptr;
    });
    const QVariantMap testProfile = m_settings->profile(profileId);
    const AiRequest request = connectionTestRequest(testProfile);
    if (m_testProvider->start(request).isEmpty()) {
        m_settings->setTestResult(QStringLiteral("failed"), tr("无法启动连接测试。"));
        m_testProvider->deleteLater();
        m_testProvider = nullptr;
    }
}

AiRequest DialogManager::connectionTestRequest(const QVariantMap &testProfile)
{
    AiRequest request;
    request.model = testProfile.value(QStringLiteral("model")).toString();
    request.stream = false;
    request.reasoningEnabled = false;
    const bool deepSeekProfile = testProfile.value(QStringLiteral("providerType")).toString()
            == QLatin1String("deepseek")
            || QUrl(testProfile.value(QStringLiteral("baseUrl")).toString()).host().compare(
                QStringLiteral("api.deepseek.com"), Qt::CaseInsensitive) == 0;
    if (deepSeekProfile) {
        request.additionalBody.insert(
                    QStringLiteral("thinking"),
                    QVariantMap{{QStringLiteral("type"),
                                 QStringLiteral("disabled")}});
    }
    request.additionalBody.insert(QStringLiteral("max_tokens"), 4);
    request.messages.append({QStringLiteral("user"), QStringLiteral("OK")});
    return request;
}

void DialogManager::setResponseGenerated(bool value)
{
    if (m_responseGenerated == value) return;
    m_responseGenerated = value;
    emit responseGeneratedChanged();
}

void DialogManager::setRequestState(const QString &state)
{
    if (m_requestState == state) return;
    m_requestState = state;
    emit requestStateChanged();
}

void DialogManager::setError(const QString &code, const QString &message)
{
    if (m_lastErrorCode == code && m_lastErrorMessage == message
            && m_lastErrorAction.isEmpty() && m_lastProviderErrorCode.isEmpty()
            && m_lastProviderErrorType.isEmpty() && m_retryAfterSeconds < 0
            && !m_canRetry) return;
    m_lastErrorCode = code;
    m_lastErrorMessage = message;
    m_lastErrorAction.clear();
    m_lastProviderErrorCode.clear();
    m_lastProviderErrorType.clear();
    m_retryAfterSeconds = -1;
    m_canRetry = false;
    emit lastErrorChanged();
}

void DialogManager::setAiError(const AiError &error, bool partial)
{
    const QString code = errorCodeName(error.code);
    QString message;
    switch (error.code) {
    case AiErrorCode::Offline: message = tr("设备似乎处于离线状态，请检查网络连接。"); break;
    case AiErrorCode::DnsFailure: message = tr("无法解析 Provider 主机名，请检查网络或接口地址。"); break;
    case AiErrorCode::ConnectionFailure: message = tr("无法连接 Provider，请检查网络后重试。"); break;
    case AiErrorCode::TlsFailure: message = tr("TLS 安全连接校验失败，请检查证书或系统时间。"); break;
    case AiErrorCode::ProxyFailure: message = tr("系统代理连接失败，请检查代理设置。"); break;
    case AiErrorCode::Timeout: message = tr("请求超时，请检查网络后重试。"); break;
    case AiErrorCode::AuthenticationFailed: message = tr("API Key 无效或已过期，请打开设置更新凭据。"); break;
    case AiErrorCode::AuthorizationFailed: message = tr("当前凭据没有执行此请求的权限，请检查 Provider 设置。"); break;
    case AiErrorCode::NotFound: message = tr("接口路径或模型不存在，请检查 Provider 配置。"); break;
    case AiErrorCode::RateLimited:
        message = error.retryAfterSeconds >= 0
                ? tr("请求过于频繁，请在 %1 秒后重试。").arg(error.retryAfterSeconds)
                : tr("请求过于频繁，请稍后重试。");
        break;
    case AiErrorCode::HttpServerError: message = tr("Provider 服务暂时不可用，请稍后重试。"); break;
    case AiErrorCode::ProtocolError: message = tr("Provider 返回了不完整或不兼容的响应。"); break;
    case AiErrorCode::ParseError: message = tr("Provider 返回的数据无法解析。"); break;
    case AiErrorCode::Cancelled: message = tr("已取消生成。"); break;
    default: message = tr("AI 请求失败，请检查配置或稍后重试。"); break;
    }
    const bool settingsAction = error.code == AiErrorCode::AuthenticationFailed
            || error.code == AiErrorCode::AuthorizationFailed
            || error.code == AiErrorCode::NotFound;
    const bool safeRetry = error.retryable && !partial
            && error.code != AiErrorCode::Cancelled;

    m_lastErrorCode = code;
    m_lastErrorMessage = message;
    m_lastErrorAction = settingsAction ? QStringLiteral("settings")
                                       : safeRetry ? QStringLiteral("retry") : QString();
    m_lastProviderErrorCode = safeDiagnosticToken(error.providerCode);
    m_lastProviderErrorType = safeDiagnosticToken(error.providerType);
    m_retryAfterSeconds = error.retryAfterSeconds;
    m_canRetry = safeRetry;
    emit lastErrorChanged();
}

QString DialogManager::errorCodeName(AiErrorCode code)
{
    switch (code) {
    case AiErrorCode::InvalidUrl: return QStringLiteral("invalid_url");
    case AiErrorCode::UnsupportedScheme: return QStringLiteral("unsupported_scheme");
    case AiErrorCode::InsecureUrl: return QStringLiteral("insecure_url");
    case AiErrorCode::Offline: return QStringLiteral("offline");
    case AiErrorCode::DnsFailure: return QStringLiteral("dns_failure");
    case AiErrorCode::ConnectionFailure: return QStringLiteral("connection_failure");
    case AiErrorCode::TlsFailure: return QStringLiteral("tls_failure");
    case AiErrorCode::ProxyFailure: return QStringLiteral("proxy_failure");
    case AiErrorCode::Timeout: return QStringLiteral("timeout");
    case AiErrorCode::Cancelled: return QStringLiteral("cancelled");
    case AiErrorCode::RedirectBlocked: return QStringLiteral("redirect_blocked");
    case AiErrorCode::AuthenticationFailed: return QStringLiteral("authentication_failed");
    case AiErrorCode::AuthorizationFailed: return QStringLiteral("authorization_failed");
    case AiErrorCode::NotFound: return QStringLiteral("not_found");
    case AiErrorCode::RateLimited: return QStringLiteral("rate_limited");
    case AiErrorCode::HttpClientError: return QStringLiteral("http_client_error");
    case AiErrorCode::HttpServerError: return QStringLiteral("http_server_error");
    case AiErrorCode::ProtocolError: return QStringLiteral("protocol_error");
    case AiErrorCode::ParseError: return QStringLiteral("parse_error");
    case AiErrorCode::Busy: return QStringLiteral("busy");
    case AiErrorCode::LegacyDisabled: return QStringLiteral("legacy_disabled");
    default: return QStringLiteral("network_failure");
    }
}
