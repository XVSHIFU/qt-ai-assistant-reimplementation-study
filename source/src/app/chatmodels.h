#pragma once

#include <QAbstractListModel>
#include <QVector>

struct ChatMessage
{
    ChatMessage() = default;
    ChatMessage(int senderValue, const QString &contentValue,
                const QString &reasoningValue = QString(),
                const QString &referenceValue = QString(),
                const QString &statusValue = QString())
        : sender(senderValue), content(contentValue), reasoningContent(reasoningValue),
          reference(referenceValue), status(statusValue) {}

    int sender = 0;
    QString content;
    QString reasoningContent;
    QString reference;
    QString status;
    QString displayErrorCode;
    QString displayErrorMessage;
    QString errorAction;
    int retryAfterSeconds = -1;
    bool partial = false;
    bool neutralError = false;
};

class ChatConversationModel final : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Role {
        SenderRole = Qt::UserRole + 1,
        ContentRole,
        ReasonModeActiveRole,
        ReasoningContentRole,
        ReferenceRole,
        StatusRole,
        DisplayErrorCodeRole,
        DisplayErrorMessageRole,
        ErrorActionRole,
        RetryAfterSecondsRole,
        PartialRole,
        NeutralErrorRole
    };

    explicit ChatConversationModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent) const override;
    Q_INVOKABLE int rowCount() const;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void clearAll();
    int append(const ChatMessage &message);
    bool removeAt(int row);
    bool appendDelta(int row, const QString &content, const QString &reasoning,
                     const QString &reference);
    bool setStatus(int row, const QString &status);
    bool setFailure(int row, const QString &status, const QString &code,
                    const QString &message, const QString &action,
                    int retryAfterSeconds, bool partial, bool neutral);
    bool clearFailure(int row, const QString &status);
    const ChatMessage *messageAt(int row) const;
    QVector<ChatMessage> messages() const { return m_items; }
    void replaceAll(const QVector<ChatMessage> &messages);

signals:
    void appended();

private:
    QVector<ChatMessage> m_items;
};

struct ConversationInfo
{
    ConversationInfo() = default;
    ConversationInfo(const QString &idValue, const QString &titleValue, bool pinnedValue = false,
                     const QString &modelValue = QString(),
                     const QString &updatedAtValue = QString(),
                     const QString &deletedAtValue = QString())
        : id(idValue), title(titleValue), pinned(pinnedValue), model(modelValue),
          updatedAt(updatedAtValue), deletedAt(deletedAtValue) {}

    QString id;
    QString title;
    bool pinned = false;
    QString model;
    QString updatedAt;
    QString deletedAt;
};

class ConversationListModel final : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Role {
        TitleRole = Qt::UserRole + 1, IdRole, PinnedRole, ModelRole,
        UpdatedAtRole, DeletedAtRole, HistorySectionRole
    };

    explicit ConversationListModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent) const override;
    Q_INVOKABLE int rowCount() const;
    QVariant data(const QModelIndex &index, int role) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    QHash<int, QByteArray> roleNames() const override;

    int append(const ConversationInfo &conversation);
    bool removeAt(int row);
    bool setTitle(int row, const QString &title);
    bool setPinned(int row, bool pinned);
    QString idAt(int row) const;
    QString titleAt(int row) const;
    bool pinnedAt(int row) const;
    int indexOf(const QString &id) const;
    void replaceAll(const QVector<ConversationInfo> &conversations);
    void appendAll(const QVector<ConversationInfo> &conversations);

private:
    QVector<ConversationInfo> m_items;
};
