#pragma once

#include "chatmodels.h"
#include "ai/aitypes.h"

#include <QObject>
#include <QDateTime>
#include <QElapsedTimer>
#include <QPointer>
#include <QTimer>

namespace smartkey {
class ProviderSettings;
class PrivacyConsentService;
class ChatStorage;
struct ConversationRecord;
namespace ai { class OpenAiCompatibleProvider; }
}

class DialogManager final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool reasonModeActive READ reasonModeActive WRITE setReasonModeActive NOTIFY reasonModeActiveChanged)
    Q_PROPERTY(bool searchModeActive READ searchModeActive WRITE setSearchModeActive NOTIFY searchModeActiveChanged)
    Q_PROPERTY(bool searchModeAvailable READ searchModeAvailable NOTIFY searchModeAvailableChanged)
    Q_PROPERTY(bool responseGenerated READ responseGenerated NOTIFY responseGeneratedChanged)
    Q_PROPERTY(bool isChatEmpty READ isChatEmpty NOTIFY chatStateChanged)
    Q_PROPERTY(bool isHistoryEmpty READ isHistoryEmpty NOTIFY historyStateChanged)
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged)
    Q_PROPERTY(QObject *dialogModel READ dialogModel CONSTANT)
    Q_PROPERTY(QObject *dialogInfoModel READ dialogInfoModel CONSTANT)
    Q_PROPERTY(QString requestState READ requestState NOTIFY requestStateChanged)
    Q_PROPERTY(QString lastErrorCode READ lastErrorCode NOTIFY lastErrorChanged)
    Q_PROPERTY(QString lastErrorMessage READ lastErrorMessage NOTIFY lastErrorChanged)
    Q_PROPERTY(QString lastErrorAction READ lastErrorAction NOTIFY lastErrorChanged)
    Q_PROPERTY(QString lastProviderErrorCode READ lastProviderErrorCode NOTIFY lastErrorChanged)
    Q_PROPERTY(QString lastProviderErrorType READ lastProviderErrorType NOTIFY lastErrorChanged)
    Q_PROPERTY(int retryAfterSeconds READ retryAfterSeconds NOTIFY lastErrorChanged)
    Q_PROPERTY(bool canRetry READ canRetry NOTIFY lastErrorChanged)
    Q_PROPERTY(QString retryPolicy READ retryPolicy CONSTANT)
    Q_PROPERTY(bool storageAvailable READ storageAvailable NOTIFY storageStateChanged)
    Q_PROPERTY(bool storageUnavailable READ storageUnavailable NOTIFY storageStateChanged)
    Q_PROPERTY(bool temporaryConversation READ temporaryConversation NOTIFY storageStateChanged)
    Q_PROPERTY(QString storageErrorMessage READ storageErrorMessage NOTIFY storageStateChanged)
    Q_PROPERTY(bool truncationOccurred READ truncationOccurred NOTIFY contextBudgetChanged)
    Q_PROPERTY(QString contextSummary READ contextSummary NOTIFY contextBudgetChanged)
    Q_PROPERTY(bool historyHasMore READ historyHasMore NOTIFY historyStateChanged)
    Q_PROPERTY(bool historyDeletedOnly READ historyDeletedOnly NOTIFY historyStateChanged)
    Q_PROPERTY(QString historyOperationMessage READ historyOperationMessage NOTIFY historyOperationChanged)

public:
    DialogManager(smartkey::ProviderSettings *settings,
                  smartkey::ai::OpenAiCompatibleProvider *provider,
                  smartkey::ChatStorage *storage,
                  QObject *parent = nullptr);
    ~DialogManager() override;

    bool reasonModeActive() const { return m_reasonModeActive; }
    bool searchModeActive() const { return m_searchModeActive; }
    bool searchModeAvailable() const { return m_searchModeAvailable; }
    bool responseGenerated() const { return m_responseGenerated; }
    bool isChatEmpty() const { return m_dialogModel.rowCount() == 0; }
    bool isHistoryEmpty() const { return m_dialogInfoModel.rowCount() == 0; }
    int currentIndex() const { return m_currentIndex; }
    QObject *dialogModel() { return &m_dialogModel; }
    QObject *dialogInfoModel() { return &m_dialogInfoModel; }
    QString requestState() const { return m_requestState; }
    QString lastErrorCode() const { return m_lastErrorCode; }
    QString lastErrorMessage() const { return m_lastErrorMessage; }
    QString lastErrorAction() const { return m_lastErrorAction; }
    QString lastProviderErrorCode() const { return m_lastProviderErrorCode; }
    QString lastProviderErrorType() const { return m_lastProviderErrorType; }
    int retryAfterSeconds() const { return m_retryAfterSeconds; }
    bool canRetry() const { return m_canRetry; }
    QString retryPolicy() const { return QStringLiteral("manual_before_first_delta_only"); }
    bool storageAvailable() const { return m_storageAvailable; }
    bool storageUnavailable() const { return !m_storageAvailable; }
    // The safe policy is fail-closed; no unpersisted temporary conversation is created.
    bool temporaryConversation() const { return false; }
    QString storageErrorMessage() const { return m_storageErrorMessage; }
    bool truncationOccurred() const { return m_truncationOccurred; }
    QString contextSummary() const { return m_contextSummary; }
    bool historyHasMore() const { return m_historyHasMore; }
    bool historyDeletedOnly() const { return m_historyDeletedOnly; }
    QString historyOperationMessage() const { return m_historyOperationMessage; }

    void setReasonModeActive(bool value);
    void setSearchModeActive(bool value);
    void setCurrentIndex(int value);
    void setPrivacyConsentService(smartkey::PrivacyConsentService *service);

    static smartkey::ai::AiRequest connectionTestRequest(const QVariantMap &profile);

    Q_INVOKABLE bool addNewChat();
    Q_INVOKABLE bool sendMessage(const QString &message);
    Q_INVOKABLE bool regenerate();
    Q_INVOKABLE bool retryLastRequest();
    Q_INVOKABLE void cancelResponse();
    Q_INVOKABLE void copy(int index);
    Q_INVOKABLE bool deleteChatAtIndex(int index);
    Q_INVOKABLE bool modifyTitleAtIndex(const QString &title, int index);
    Q_INVOKABLE void selectChatById(const QString &conversationId);
    Q_INVOKABLE bool deleteChatById(const QString &conversationId);
    Q_INVOKABLE bool renameChatById(const QString &conversationId, const QString &title);
    Q_INVOKABLE bool setChatPinned(const QString &conversationId, bool pinned);
    Q_INVOKABLE bool applyHistoryFilter(const QString &text, const QString &model,
                                        const QString &fromIso, const QString &toIso,
                                        bool deletedOnly);
    Q_INVOKABLE bool loadMoreHistory();
    Q_INVOKABLE bool restoreChatById(const QString &conversationId);
    Q_INVOKABLE bool purgeDeletedChatById(const QString &conversationId);
    Q_INVOKABLE bool purgeExpiredDeletedHistory();
    Q_INVOKABLE bool exportChatById(const QString &conversationId,
                                    const QString &directoryUrl, const QString &format);
    Q_INVOKABLE bool exportFilteredHistory(const QString &directoryUrl,
                                           const QString &format);
    Q_INVOKABLE void parseJSONToCurrentChat();
    Q_INVOKABLE void reloadHistory();

    // Ordered application-shutdown phases. These methods are idempotent and
    // keep request cancellation separate from the final persistence fallback.
    void stopAcceptingRequests();
    void cancelRequestsForShutdown();
    void flushAndPersistShutdownState();

signals:
    void reasonModeActiveChanged();
    void searchModeActiveChanged();
    void searchModeAvailableChanged();
    void responseGeneratedChanged();
    void chatStateChanged();
    void historyStateChanged();
    void currentIndexChanged();
    void requestStateChanged();
    void lastErrorChanged();
    void storageStateChanged();
    void contextBudgetChanged();
    void historyOperationChanged();
    void appended();

private slots:
    bool flushPendingDeltas();
    void handleCompleted(const QString &requestId);
    void handleFailed(const QString &requestId, const smartkey::ai::AiError &error);
    void testConnection(const QString &profileId);

private:
    bool configureProvider(const QString &profileId,
                           smartkey::ai::OpenAiCompatibleProvider *provider,
                           QString *errorMessage);
    bool ensurePrivacyConsent(const QString &action);
    bool ensureStorageAvailable(const QString &operation);
    bool handleStorageFailure(const QString &operation);
    bool failPreparedAssistant(const QString &code, const QString &message);
    void startCurrentConversation();
    void syncProviderCapabilities();
    void setResponseGenerated(bool value);
    void setRequestState(const QString &state);
    void setError(const QString &code, const QString &message);
    void setAiError(const smartkey::ai::AiError &error, bool partial);
    void loadCurrentConversation();
    void reloadConversationList(const QString &preferredConversationId = QString());
    bool persistConversation();
    void setContextBudgetState(bool truncated, const QString &summary);
    void noteFirstDelta(const QString &requestId);
    bool beginRequestAttempt(const QString &requestId, qint64 estimatedInputTokens);
    bool updateRequestAttempt(const QString &status,
                              const QString &errorCode = QString(), int httpStatus = 0);
    smartkey::ConversationRecord currentConversationRecord(const QString &id,
                                                           const QString &title) const;
    static QString errorCodeName(smartkey::ai::AiErrorCode code);

    smartkey::ProviderSettings *m_settings = nullptr;
    smartkey::ai::OpenAiCompatibleProvider *m_provider = nullptr;
    smartkey::ChatStorage *m_storage = nullptr;
    smartkey::PrivacyConsentService *m_privacyConsent = nullptr;
    smartkey::ai::OpenAiCompatibleProvider *m_testProvider = nullptr;
    ChatConversationModel m_dialogModel;
    ConversationListModel m_dialogInfoModel;
    bool m_reasonModeActive = false;
    bool m_searchModeActive = false;
    bool m_searchModeAvailable = false;
    bool m_responseGenerated = true;
    bool m_acceptingRequests = true;
    bool m_storageAvailable = false;
    bool m_truncationOccurred = false;
    int m_currentIndex = -1;
    int m_streamingRow = -1;
    QString m_requestId;
    QString m_requestState = QStringLiteral("idle");
    QString m_lastErrorCode;
    QString m_lastErrorMessage;
    QString m_lastErrorAction;
    QString m_lastProviderErrorCode;
    QString m_lastProviderErrorType;
    int m_retryAfterSeconds = -1;
    bool m_canRetry = false;
    QString m_storageErrorMessage;
    QString m_contextSummary;
    QString m_historyFilterText;
    QString m_historyModelFilter;
    QDateTime m_historyFrom;
    QDateTime m_historyTo;
    bool m_historyDeletedOnly = false;
    bool m_historyHasMore = false;
    int m_historyCursorPinned = -1;
    QString m_historyCursorUpdatedAt;
    QString m_historyCursorId;
    QString m_historyOperationMessage;
    QString m_attemptId;
    QString m_attemptRequestId;
    QString m_attemptConversationId;
    QString m_attemptMessageId;
    QString m_attemptProfileId;
    qint64 m_attemptFirstDeltaMs = -1;
    qint64 m_attemptInputTokens = -1;
    qint64 m_attemptOutputTokens = -1;
    QDateTime m_attemptCreatedAt;
    QElapsedTimer m_attemptTimer;
    QString m_pendingContent;
    QString m_pendingReasoning;
    QString m_pendingReference;
    QStringList m_messageIds;
    QTimer m_deltaTimer;
};
