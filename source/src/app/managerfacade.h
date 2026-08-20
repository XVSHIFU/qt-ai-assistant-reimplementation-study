#pragma once

#include <QObject>

class ManagerFacade final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString currentTheme READ currentTheme CONSTANT)
public:
    explicit ManagerFacade(QObject *parent = nullptr) : QObject(parent) {}
    QString currentTheme() const { return QStringLiteral("LightTheme.qml"); }
    Q_INVOKABLE void itemClicked(int buttonIndex) { emit itemSelected(buttonIndex); }
signals:
    void itemSelected(int buttonIndex);
};
