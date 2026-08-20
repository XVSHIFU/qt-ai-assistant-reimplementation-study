#pragma once

#include <QAbstractListModel>
#include <QVariantMap>
#include <QVector>

class ChatMessageModel final : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Role {
        SenderRole = Qt::UserRole + 1,
        ContentRole,
        ReasonModeActiveRole,
        ReasoningContentRole,
        ReferenceRole
    };

    explicit ChatMessageModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent) const override;
    Q_INVOKABLE int rowCount() const;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    Q_INVOKABLE void clearAll();
    void append(int sender, const QString &content, const QString &reasoning = QString(),
                const QString &reference = QString());
signals:
    void appended();
private:
    struct Item {
        int sender = 0;
        QString content;
        bool reasonModeActive = false;
        QString reasoningContent;
        QString reference;
    };
    QVector<Item> m_items;
};

class DialogInfoModel final : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Role { TitleRole = Qt::UserRole + 1 };
    explicit DialogInfoModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent) const override;
    Q_INVOKABLE int rowCount() const;
    QVariant data(const QModelIndex &index, int role) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    QHash<int, QByteArray> roleNames() const override;
    void append(const QString &title);
    void removeAt(int index);
    void setTitle(int index, const QString &title);
private:
    QVector<QString> m_titles;
};

class DeviceModel final : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Role {
        DeviceTypeRole = Qt::UserRole + 1,
        DeviceNameRole,
        BatteryValueRole,
        ConnectTypeRole
    };
    explicit DeviceModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent) const override;
    Q_INVOKABLE int rowCount() const;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    Q_INVOKABLE QVariantMap get(int index) const;
private:
    struct Item {
        int deviceType;
        QString deviceName;
        int batteryValue;
        QString connectType;
    };
    QVector<Item> m_items;
};

