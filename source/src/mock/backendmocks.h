#pragma once

#include "models.h"

#include <QObject>
#include <QString>

class WindowMock final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(qreal x READ x WRITE setX NOTIFY geometryChanged)
    Q_PROPERTY(qreal y READ y WRITE setY NOTIFY geometryChanged)
    Q_PROPERTY(qreal width READ width WRITE setWidth NOTIFY geometryChanged)
    Q_PROPERTY(qreal height READ height WRITE setHeight NOTIFY geometryChanged)
    Q_PROPERTY(qreal scale READ scale CONSTANT)
    Q_PROPERTY(bool agreePolicy READ agreePolicy WRITE setAgreePolicy NOTIFY agreePolicyChanged)
    Q_PROPERTY(bool allowQuestion READ allowQuestion CONSTANT)
    Q_PROPERTY(bool notEnough READ notEnough CONSTANT)
    Q_PROPERTY(QString newVersionNumber READ newVersionNumber CONSTANT)
    Q_PROPERTY(bool topScreen READ topScreen WRITE setTopScreen NOTIFY topScreenChanged)
    Q_PROPERTY(qreal radius READ radius CONSTANT)
    Q_PROPERTY(int skinMode READ skinMode WRITE setSkinMode NOTIFY skinModeChanged)
    Q_PROPERTY(int sysSkinType READ sysSkinType CONSTANT)
public:
    explicit WindowMock(QObject *parent = nullptr);
    qreal x() const { return m_x; }
    qreal y() const { return m_y; }
    qreal width() const { return m_width; }
    qreal height() const { return m_height; }
    qreal scale() const { return 1.0; }
    bool agreePolicy() const { return m_agreePolicy; }
    bool allowQuestion() const { return true; }
    bool notEnough() const { return false; }
    QString newVersionNumber() const { return QStringLiteral("5.15.2-mock"); }
    bool topScreen() const { return m_topScreen; }
    qreal radius() const { return 12.0; }
    int skinMode() const { return m_skinMode; }
    int sysSkinType() const { return 0; }
    void setX(qreal value);
    void setY(qreal value);
    void setWidth(qreal value);
    void setHeight(qreal value);
    void setAgreePolicy(bool value);
    void setTopScreen(bool value);
    void setSkinMode(int value);
    Q_INVOKABLE void openWebsite(const QString &url);
    Q_INVOKABLE void setDialog(bool open);
    Q_INVOKABLE void setFirstStart(bool firstStart);
    Q_INVOKABLE void setHistory(bool open);
    Q_INVOKABLE void setXandY(qreal x, qreal y, qreal width, qreal height);
    Q_INVOKABLE void showGuideWindow();
    Q_INVOKABLE void updateSoft();
signals:
    void geometryChanged();
    void agreePolicyChanged();
    void topScreenChanged();
    void skinModeChanged();
private:
    qreal m_x = 0;
    qreal m_y = 0;
    qreal m_width = 576;
    qreal m_height = 720;
    bool m_agreePolicy = true;
    bool m_topScreen = false;
    int m_skinMode = 0;
};

class DialogManagerMock final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool reasonModeActive READ reasonModeActive WRITE setReasonModeActive NOTIFY reasonModeActiveChanged)
    Q_PROPERTY(bool searchModeActive READ searchModeActive WRITE setSearchModeActive NOTIFY searchModeActiveChanged)
    Q_PROPERTY(bool responseGenerated READ responseGenerated NOTIFY responseGeneratedChanged)
    Q_PROPERTY(bool isChatEmpty READ isChatEmpty NOTIFY chatStateChanged)
    Q_PROPERTY(bool isHistoryEmpty READ isHistoryEmpty NOTIFY historyStateChanged)
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged)
    Q_PROPERTY(QObject *dialogModel READ dialogModel CONSTANT)
    Q_PROPERTY(QObject *dialogInfoModel READ dialogInfoModel CONSTANT)
public:
    explicit DialogManagerMock(QObject *parent = nullptr);
    bool reasonModeActive() const { return m_reasonModeActive; }
    bool searchModeActive() const { return m_searchModeActive; }
    bool responseGenerated() const { return m_responseGenerated; }
    bool isChatEmpty() const { return m_dialogModel.rowCount() == 0; }
    bool isHistoryEmpty() const { return m_dialogInfoModel.rowCount() == 0; }
    int currentIndex() const { return m_currentIndex; }
    QObject *dialogModel() { return &m_dialogModel; }
    QObject *dialogInfoModel() { return &m_dialogInfoModel; }
    void setReasonModeActive(bool value);
    void setSearchModeActive(bool value);
    void setCurrentIndex(int value);
    Q_INVOKABLE void addNewChat();
    Q_INVOKABLE void sendMessage(const QString &message);
    Q_INVOKABLE void regenerate();
    Q_INVOKABLE void copy(int index);
    Q_INVOKABLE void deleteChatAtIndex(int index);
    Q_INVOKABLE void modifyTitleAtIndex(const QString &title, int index);
    Q_INVOKABLE void parseJSONToCurrentChat();
signals:
    void reasonModeActiveChanged();
    void searchModeActiveChanged();
    void responseGeneratedChanged();
    void chatStateChanged();
    void historyStateChanged();
    void currentIndexChanged();
    void appended();
private:
    ChatMessageModel m_dialogModel;
    DialogInfoModel m_dialogInfoModel;
    bool m_reasonModeActive = false;
    bool m_searchModeActive = false;
    bool m_responseGenerated = true;
    int m_currentIndex = 0;
};

class SettingMock final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool updated READ updated NOTIFY updatedChanged)
    Q_PROPERTY(QString currentVersion READ currentVersion CONSTANT)
    Q_PROPERTY(bool startUpAuto READ startUpAuto WRITE setStartUpAuto NOTIFY startUpAutoChanged)
    Q_PROPERTY(int totalCount READ totalCount CONSTANT)
    Q_PROPERTY(int usedCount READ usedCount CONSTANT)
    Q_PROPERTY(int usedNumCount READ usedNumCount CONSTANT)
public:
    explicit SettingMock(QObject *parent = nullptr) : QObject(parent) {}
    bool updated() const { return m_updated; }
    QString currentVersion() const { return QStringLiteral("1.0.0-reconstruction"); }
    bool startUpAuto() const { return m_startUpAuto; }
    int totalCount() const { return 100; }
    int usedCount() const { return 24; }
    int usedNumCount() const { return 24; }
    void setStartUpAuto(bool value);
    Q_INVOKABLE void checkUpdate();
    Q_INVOKABLE void checkUpdate(bool interactive);
signals:
    void updatedChanged();
    void startUpAutoChanged();
private:
    bool m_updated = false;
    bool m_startUpAuto = false;
};

class ManagerMock final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString currentTheme READ currentTheme CONSTANT)
public:
    explicit ManagerMock(QObject *parent = nullptr) : QObject(parent) {}
    QString currentTheme() const { return QStringLiteral("LightTheme.qml"); }
    Q_INVOKABLE void itemClicked(int buttonIndex);
signals:
    void itemSelected(int buttonIndex);
};

class DeviceListMock final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QObject *deviceModel READ deviceModel CONSTANT)
public:
    explicit DeviceListMock(QObject *parent = nullptr) : QObject(parent), m_deviceModel(this) {}
    QObject *deviceModel() { return &m_deviceModel; }
    Q_INVOKABLE QString reCurConfigure() const { return QStringLiteral("Demo profile"); }
signals:
    void deviceListChanged();
    void deviceDisconnection();
private:
    DeviceModel m_deviceModel;
};

