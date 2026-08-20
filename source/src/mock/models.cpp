#include "models.h"

ChatMessageModel::ChatMessageModel(QObject *parent)
    : QAbstractListModel(parent)
{
    append(0, QStringLiteral("How can SmartKey help me?"));
    append(1,
           QStringLiteral("This reconstruction is running with a local demonstration backend."),
           QStringLiteral("No network request was made."),
           QStringLiteral("Mock response"));
}

int ChatMessageModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_items.size();
}

int ChatMessageModel::rowCount() const
{
    return m_items.size();
}

QVariant ChatMessageModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size())
        return {};
    const Item &item = m_items.at(index.row());
    switch (role) {
    case SenderRole: return item.sender;
    case ContentRole: return item.content;
    case ReasonModeActiveRole: return item.reasonModeActive;
    case ReasoningContentRole: return item.reasoningContent;
    case ReferenceRole: return item.reference;
    default: return {};
    }
}

QHash<int, QByteArray> ChatMessageModel::roleNames() const
{
    return {
        {SenderRole, "sender"},
        {ContentRole, "content"},
        {ReasonModeActiveRole, "reasonModeActive"},
        {ReasoningContentRole, "reasoningContent"},
        {ReferenceRole, "reference"}
    };
}

void ChatMessageModel::clearAll()
{
    beginResetModel();
    m_items.clear();
    endResetModel();
}

void ChatMessageModel::append(int sender, const QString &content, const QString &reasoning,
                              const QString &reference)
{
    const int position = m_items.size();
    beginInsertRows(QModelIndex(), position, position);
    Item item;
    item.sender = sender;
    item.content = content;
    item.reasonModeActive = !reasoning.isEmpty();
    item.reasoningContent = reasoning;
    item.reference = reference;
    m_items.append(item);
    endInsertRows();
    emit appended();
}

DialogInfoModel::DialogInfoModel(QObject *parent)
    : QAbstractListModel(parent), m_titles({QStringLiteral("Demo chat")})
{
}

int DialogInfoModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_titles.size();
}

int DialogInfoModel::rowCount() const
{
    return m_titles.size();
}

QVariant DialogInfoModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_titles.size() || role != TitleRole)
        return {};
    return m_titles.at(index.row());
}

bool DialogInfoModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_titles.size() || role != TitleRole)
        return false;
    m_titles[index.row()] = value.toString();
    emit dataChanged(index, index, {role});
    return true;
}

QHash<int, QByteArray> DialogInfoModel::roleNames() const
{
    return {{TitleRole, "title"}};
}

void DialogInfoModel::append(const QString &title)
{
    const int position = m_titles.size();
    beginInsertRows(QModelIndex(), position, position);
    m_titles.append(title);
    endInsertRows();
}

void DialogInfoModel::removeAt(int index)
{
    if (index < 0 || index >= m_titles.size())
        return;
    beginRemoveRows(QModelIndex(), index, index);
    m_titles.removeAt(index);
    endRemoveRows();
}

void DialogInfoModel::setTitle(int index, const QString &title)
{
    if (index < 0 || index >= m_titles.size())
        return;
    m_titles[index] = title;
    const QModelIndex changed = createIndex(index, 0);
    emit dataChanged(changed, changed, {TitleRole});
}

DeviceModel::DeviceModel(QObject *parent)
    : QAbstractListModel(parent)
{
    m_items.append({0, QStringLiteral("Rapoo demo keyboard"), 86, QStringLiteral("wireless")});
}

int DeviceModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_items.size();
}

int DeviceModel::rowCount() const
{
    return m_items.size();
}

QVariant DeviceModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size())
        return {};
    const Item &item = m_items.at(index.row());
    switch (role) {
    case DeviceTypeRole: return item.deviceType;
    case DeviceNameRole: return item.deviceName;
    case BatteryValueRole: return item.batteryValue;
    case ConnectTypeRole: return item.connectType;
    default: return {};
    }
}

QHash<int, QByteArray> DeviceModel::roleNames() const
{
    return {
        {DeviceTypeRole, "deviceType"},
        {DeviceNameRole, "deviceName"},
        {BatteryValueRole, "batteryValue"},
        {ConnectTypeRole, "connectType"}
    };
}

QVariantMap DeviceModel::get(int index) const
{
    QVariantMap result;
    if (index < 0 || index >= m_items.size())
        return result;
    const Item &item = m_items.at(index);
    result.insert(QStringLiteral("deviceType"), item.deviceType);
    result.insert(QStringLiteral("deviceName"), item.deviceName);
    result.insert(QStringLiteral("batteryValue"), item.batteryValue);
    result.insert(QStringLiteral("connectType"), item.connectType);
    return result;
}
