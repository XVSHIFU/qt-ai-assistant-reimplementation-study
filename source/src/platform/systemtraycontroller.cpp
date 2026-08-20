#include "systemtraycontroller.h"

#include "autostartservice.h"
#include "windowcontroller.h"

#include <QAction>
#include <QIcon>
#include <QMenu>
#include <QSystemTrayIcon>

SystemTrayController::SystemTrayController(WindowController *windowController,
                                           AutoStartService *autoStartService,
                                           QObject *parent)
    : QObject(parent),
      m_windowController(windowController),
      m_autoStartService(autoStartService),
      m_trayIcon(new QSystemTrayIcon(this)),
      m_menu(new QMenu)
{
    m_trayIcon->setIcon(QIcon(QStringLiteral(":/Image/Log/com.rapoo.smartkey.ico")));
    m_trayIcon->setToolTip(tr("智键 AI"));

    QAction *showAction = m_menu->addAction(tr("显示智键 AI"));
    QAction *newChatAction = m_menu->addAction(tr("新建对话"));
    QAction *settingsAction = m_menu->addAction(tr("设置"));
    m_autoStartAction = m_menu->addAction(tr("开机自启动"));
    m_autoStartAction->setCheckable(true);
    m_menu->addSeparator();
    QAction *exitAction = m_menu->addAction(tr("退出"));
    m_trayIcon->setContextMenu(m_menu);

    connect(showAction, &QAction::triggered, this, &SystemTrayController::showRequested);
    connect(newChatAction, &QAction::triggered, this, &SystemTrayController::newChatRequested);
    connect(settingsAction, &QAction::triggered, this, &SystemTrayController::settingsRequested);
    connect(exitAction, &QAction::triggered, this, &SystemTrayController::exitRequested);
    connect(m_trayIcon, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
        handleActivation(static_cast<int>(reason));
    });

    if (m_windowController) {
        connect(this, &SystemTrayController::showRequested,
                m_windowController, &WindowController::reveal);
        connect(this, &SystemTrayController::toggleRequested,
                m_windowController, &WindowController::toggleFromTray);
    }

    if (m_autoStartService) {
        syncAutoStartAction();
        connect(m_autoStartAction, &QAction::toggled, this, [this](bool enabled) {
            if (!m_autoStartService)
                return;
            QString error;
            if (!m_autoStartService->setEnabled(enabled, &error)) {
                m_autoStartAction->blockSignals(true);
                m_autoStartAction->setChecked(m_autoStartService->isEnabled());
                m_autoStartAction->blockSignals(false);
                emit autoStartUpdateFailed(error);
            }
        });
        connect(m_autoStartService, &AutoStartService::enabledChanged,
                this, &SystemTrayController::syncAutoStartAction);
    } else {
        m_autoStartAction->setVisible(false);
    }
}

SystemTrayController::~SystemTrayController()
{
    hide();
    if (m_trayIcon)
        m_trayIcon->setContextMenu(nullptr);
    delete m_menu;
    m_menu = nullptr;
}

bool SystemTrayController::isAvailable() const
{
    return QSystemTrayIcon::isSystemTrayAvailable();
}

bool SystemTrayController::isVisible() const
{
    return m_trayIcon && m_trayIcon->isVisible();
}

void SystemTrayController::show()
{
    if (!m_trayIcon || m_trayIcon->isVisible())
        return;
    m_trayIcon->show();
    emit visibilityChanged(true);
}

void SystemTrayController::hide()
{
    if (!m_trayIcon || !m_trayIcon->isVisible())
        return;
    m_trayIcon->hide();
    emit visibilityChanged(false);
}

void SystemTrayController::setToolTip(const QString &toolTip)
{
    if (m_trayIcon)
        m_trayIcon->setToolTip(toolTip);
}

void SystemTrayController::handleActivation(int reason)
{
    const auto activationReason = static_cast<QSystemTrayIcon::ActivationReason>(reason);
    if (activationReason == QSystemTrayIcon::Trigger
            || activationReason == QSystemTrayIcon::DoubleClick)
        emit toggleRequested();
}

void SystemTrayController::syncAutoStartAction()
{
    if (!m_autoStartAction || !m_autoStartService)
        return;
    m_autoStartAction->blockSignals(true);
    m_autoStartAction->setChecked(m_autoStartService->isEnabled());
    m_autoStartAction->blockSignals(false);
}
