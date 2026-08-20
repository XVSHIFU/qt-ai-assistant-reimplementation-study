#include "chatmodels.h"

ChatConversationModel::ChatConversationModel(QObject *parent) : QAbstractListModel(parent) {}

int ChatConversationModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_items.size();
}

int ChatConversationModel::rowCount() const { return m_items.size(); }

QVariant ChatConversationModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size())
        return {};
    const ChatMessage &item = m_items.at(index.row());
    switch (role) {
    case SenderRole: return item.sender;
    case ContentRole: return item.content;
    case ReasonModeActiveRole: return !item.reasoningContent.isEmpty();
    case ReasoningContentRole: return item.reasoningContent;
    case ReferenceRole: return item.reference;
    case StatusRole: return item.status;
    case DisplayErrorCodeRole: return item.displayErrorCode;
    case DisplayErrorMessageRole: return item.displayErrorMessage;
    case ErrorActionRole: return item.errorAction;
    case RetryAfterSecondsRole: return item.retryAfterSeconds;
    case PartialRole: return item.partial;
    case NeutralErrorRole: return item.neutralError;
    default: return {};
    }
}

QHash<int, QByteArray> ChatConversationModel::roleNames() const
{
    return {
        {SenderRole, "sender"},
        {ContentRole, "content"},
        {ReasonModeActiveRole, "reasonModeActive"},
        {ReasoningContentRole, "reasoningContent"},
        {ReferenceRole, "reference"},
        {StatusRole, "status"},
        {DisplayErrorCodeRole, "displayErrorCode"},
        {DisplayErrorMessageRole, "displayErrorMessage"},
        {ErrorActionRole, "errorAction"},
        {RetryAfterSecondsRole, "retryAfterSeconds"},
        {PartialRole, "partial"},
        {NeutralErrorRole, "neutralError"}
    };
}

void ChatConversationModel::clearAll()
{
    beginResetModel();
    m_items.clear();
    endResetModel();
}

int ChatConversationModel::append(const ChatMessage &message)
{
    const int row = m_items.size();
    beginInsertRows(QModelIndex(), row, row);
    m_items.append(message);
    endInsertRows();
    emit appended();
    return row;
}

bool ChatConversationModel::removeAt(int row)
{
    if (row < 0 || row >= m_items.size())
        return false;
    beginRemoveRows(QModelIndex(), row, row);
    m_items.removeAt(row);
    endRemoveRows();
    return true;
}

bool ChatConversationModel::appendDelta(int row, const QString &content,
                                        const QString &reasoning, const QString &reference)
{
    if (row < 0 || row >= m_items.size())
        return false;
    ChatMessage &item = m_items[row];
    item.content += content;
    item.reasoningContent += reasoning;
    item.reference += reference;
    QVector<int> roles;
    if (!content.isEmpty()) roles << ContentRole;
    if (!reasoning.isEmpty()) roles << ReasoningContentRole << ReasonModeActiveRole;
    if (!reference.isEmpty()) roles << ReferenceRole;
    if (!roles.isEmpty()) {
        const QModelIndex changed = index(row, 0);
        emit dataChanged(changed, changed, roles);
    }
    return true;
}

bool ChatConversationModel::setStatus(int row, const QString &status)
{
    if (row < 0 || row >= m_items.size() || m_items[row].status == status)
        return false;
    m_items[row].status = status;
    const QModelIndex changed = index(row, 0);
    emit dataChanged(changed, changed, {StatusRole});
    return true;
}

bool ChatConversationModel::setFailure(int row, const QString &status,
                                       const QString &code, const QString &message,
                                       const QString &action, int retryAfterSeconds,
                                       bool partial, bool neutral)
{
    if (row < 0 || row >= m_items.size())
        return false;
    ChatMessage &item = m_items[row];
    item.status = status;
    item.displayErrorCode = code;
    item.displayErrorMessage = message;
    item.errorAction = action;
    item.retryAfterSeconds = retryAfterSeconds;
    item.partial = partial;
    item.neutralError = neutral;
    const QModelIndex changed = index(row, 0);
    emit dataChanged(changed, changed,
                     {StatusRole, DisplayErrorCodeRole, DisplayErrorMessageRole,
                      ErrorActionRole, RetryAfterSecondsRole, PartialRole,
                      NeutralErrorRole});
    return true;
}

bool ChatConversationModel::clearFailure(int row, const QString &status)
{
    if (row < 0 || row >= m_items.size())
        return false;
    ChatMessage &item = m_items[row];
    item.status = status;
    item.displayErrorCode.clear();
    item.displayErrorMessage.clear();
    item.errorAction.clear();
    item.retryAfterSeconds = -1;
    item.partial = false;
    item.neutralError = false;
    const QModelIndex changed = index(row, 0);
    emit dataChanged(changed, changed,
                     {StatusRole, DisplayErrorCodeRole, DisplayErrorMessageRole,
                      ErrorActionRole, RetryAfterSecondsRole, PartialRole,
                      NeutralErrorRole});
    return true;
}

const ChatMessage *ChatConversationModel::messageAt(int row) const
{
    return row >= 0 && row < m_items.size() ? &m_items.at(row) : nullptr;
}

void ChatConversationModel::replaceAll(const QVector<ChatMessage> &messages)
{
    beginResetModel();
    m_items = messages;
    endResetModel();
}

ConversationListModel::ConversationListModel(QObject *parent) : QAbstractListModel(parent) {}

int ConversationListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_items.size();
}

int ConversationListModel::rowCount() const { return m_items.size(); }

QVariant ConversationListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size())
        return {};
    const ConversationInfo &item = m_items.at(index.row());
    if (role == TitleRole) return item.title;
    if (role == IdRole) return item.id;
    if (role == PinnedRole) return item.pinned;
    if (role == ModelRole) return item.model;
    if (role == UpdatedAtRole) return item.updatedAt;
    if (role == DeletedAtRole) return item.deletedAt;
    if (role == HistorySectionRole)
        return !item.deletedAt.isEmpty() ? QStringLiteral("deleted")
             : item.pinned ? QStringLiteral("pinned") : QStringLiteral("recent");
    return {};
}

bool ConversationListModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    return role == TitleRole && index.isValid() && setTitle(index.row(), value.toString());
}

QHash<int, QByteArray> ConversationListModel::roleNames() const
{
    return {{TitleRole, "title"}, {IdRole, "conversationId"}, {PinnedRole, "pinned"},
            {ModelRole, "conversationModel"}, {UpdatedAtRole, "updatedAt"},
            {DeletedAtRole, "deletedAt"}, {HistorySectionRole, "historySection"}};
}

int ConversationListModel::append(const ConversationInfo &conversation)
{
    const int row = m_items.size();
    beginInsertRows(QModelIndex(), row, row);
    m_items.append(conversation);
    endInsertRows();
    return row;
}

bool ConversationListModel::removeAt(int row)
{
    if (row < 0 || row >= m_items.size())
        return false;
    beginRemoveRows(QModelIndex(), row, row);
    m_items.removeAt(row);
    endRemoveRows();
    return true;
}

bool ConversationListModel::setTitle(int row, const QString &title)
{
    if (row < 0 || row >= m_items.size())
        return false;
    m_items[row].title = title;
    const QModelIndex changed = index(row, 0);
    emit dataChanged(changed, changed, {TitleRole});
    return true;
}

bool ConversationListModel::setPinned(int row, bool pinned)
{
    if (row < 0 || row >= m_items.size() || m_items[row].pinned == pinned)
        return false;
    m_items[row].pinned = pinned;
    const QModelIndex changed = index(row, 0);
    emit dataChanged(changed, changed, {PinnedRole});
    return true;
}

QString ConversationListModel::idAt(int row) const
{
    return row >= 0 && row < m_items.size() ? m_items.at(row).id : QString();
}

QString ConversationListModel::titleAt(int row) const
{
    return row >= 0 && row < m_items.size() ? m_items.at(row).title : QString();
}

bool ConversationListModel::pinnedAt(int row) const
{
    return row >= 0 && row < m_items.size() && m_items.at(row).pinned;
}

int ConversationListModel::indexOf(const QString &id) const
{
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).id == id)
            return i;
    }
    return -1;
}

void ConversationListModel::replaceAll(const QVector<ConversationInfo> &conversations)
{
    beginResetModel();
    m_items = conversations;
    endResetModel();
}

void ConversationListModel::appendAll(const QVector<ConversationInfo> &conversations)
{
    if (conversations.isEmpty())
        return;
    const int first = m_items.size();
    beginInsertRows(QModelIndex(), first, first + conversations.size() - 1);
    m_items += conversations;
    endInsertRows();
}
