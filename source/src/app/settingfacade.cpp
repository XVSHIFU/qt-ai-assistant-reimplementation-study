#include "settingfacade.h"

#include "platform/autostartservice.h"

#include <QCoreApplication>

SettingFacade::SettingFacade(AutoStartService *autoStart, QObject *parent)
    : QObject(parent), m_autoStart(autoStart)
{
    if (m_autoStart) {
        connect(m_autoStart, &AutoStartService::enabledChanged,
                this, &SettingFacade::startUpAutoChanged);
        connect(m_autoStart, &AutoStartService::updateFailed,
                this, &SettingFacade::errorOccurred);
    }
}

QString SettingFacade::currentVersion() const { return QCoreApplication::applicationVersion(); }
bool SettingFacade::startUpAuto() const { return m_autoStart && m_autoStart->isEnabled(); }

void SettingFacade::setStartUpAuto(bool value)
{
    if (!m_autoStart)
        return;
    QString error;
    if (!m_autoStart->setEnabled(value, &error) && !error.isEmpty())
        emit errorOccurred(error);
}

void SettingFacade::checkUpdate() { checkUpdate(true); }
void SettingFacade::checkUpdate(bool interactive) { emit updateCheckRequested(interactive); }
