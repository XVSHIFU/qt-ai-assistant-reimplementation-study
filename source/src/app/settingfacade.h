#pragma once

#include <QObject>

class AutoStartService;

class SettingFacade final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool updated READ updated NOTIFY updatedChanged)
    Q_PROPERTY(QString currentVersion READ currentVersion CONSTANT)
    Q_PROPERTY(bool startUpAuto READ startUpAuto WRITE setStartUpAuto NOTIFY startUpAutoChanged)
    Q_PROPERTY(int totalCount READ totalCount CONSTANT)
    Q_PROPERTY(int usedCount READ usedCount CONSTANT)
    Q_PROPERTY(int usedNumCount READ usedNumCount CONSTANT)

public:
    explicit SettingFacade(AutoStartService *autoStart, QObject *parent = nullptr);

    bool updated() const { return false; }
    QString currentVersion() const;
    bool startUpAuto() const;
    int totalCount() const { return 0; }
    int usedCount() const { return 0; }
    int usedNumCount() const { return 0; }

    Q_INVOKABLE void setStartUpAuto(bool value);
    Q_INVOKABLE void checkUpdate();
    Q_INVOKABLE void checkUpdate(bool interactive);

signals:
    void updatedChanged();
    void startUpAutoChanged();
    void updateCheckRequested(bool interactive);
    void errorOccurred(const QString &message);

private:
    AutoStartService *m_autoStart = nullptr;
};
