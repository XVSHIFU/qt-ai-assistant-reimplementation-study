#pragma once

#include <QObject>

class QAction;
class AutoStartService;
class QMenu;
class QSystemTrayIcon;
class WindowController;

class SystemTrayController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ isAvailable CONSTANT)
    Q_PROPERTY(bool visible READ isVisible NOTIFY visibilityChanged)

public:
    explicit SystemTrayController(WindowController *windowController,
                                  AutoStartService *autoStartService = nullptr,
                                  QObject *parent = nullptr);
    ~SystemTrayController() override;

    bool isAvailable() const;
    bool isVisible() const;
    QSystemTrayIcon *trayIcon() const { return m_trayIcon; }

public slots:
    void show();
    void hide();
    void setToolTip(const QString &toolTip);

signals:
    void showRequested();
    void toggleRequested();
    void settingsRequested();
    void newChatRequested();
    void exitRequested();
    void visibilityChanged(bool visible);
    void autoStartUpdateFailed(const QString &message);

private slots:
    void handleActivation(int reason);
    void syncAutoStartAction();

private:
    WindowController *m_windowController = nullptr;
    AutoStartService *m_autoStartService = nullptr;
    QSystemTrayIcon *m_trayIcon = nullptr;
    QMenu *m_menu = nullptr;
    QAction *m_autoStartAction = nullptr;
};
