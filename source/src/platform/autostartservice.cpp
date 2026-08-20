#include "autostartservice.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>

namespace {
QString quoteArgument(const QString &argument)
{
    QString escaped = argument;
    escaped.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    return QStringLiteral("\"%1\"").arg(escaped);
}
}

AutoStartService::AutoStartService(const QString &valueName, QObject *parent)
    : QObject(parent),
      m_valueName(valueName.isEmpty() ? QCoreApplication::applicationName() : valueName)
{
    if (m_valueName.isEmpty())
        m_valueName = QStringLiteral("SmartKey AI");
}

bool AutoStartService::isAvailable()
{
#ifdef Q_OS_WIN
    return true;
#else
    return false;
#endif
}

QString AutoStartService::startupCommand() const
{
    QStringList commandParts;
    commandParts << quoteArgument(QFileInfo(QCoreApplication::applicationFilePath()).absoluteFilePath());
    for (const QString &argument : m_arguments)
        commandParts << quoteArgument(argument);
    return commandParts.join(QLatin1Char(' '));
}

bool AutoStartService::isEnabled() const
{
#ifdef Q_OS_WIN
    QSettings registry(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
                       QSettings::NativeFormat);
    return registry.contains(m_valueName);
#else
    return false;
#endif
}

bool AutoStartService::setEnabled(bool enabled, QString *errorMessage)
{
#ifdef Q_OS_WIN
    const bool wasEnabled = isEnabled();
    QSettings registry(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
                       QSettings::NativeFormat);
    if (enabled)
        registry.setValue(m_valueName, startupCommand());
    else
        registry.remove(m_valueName);
    registry.sync();

    const bool success = registry.status() == QSettings::NoError
            && registry.contains(m_valueName) == enabled;
    if (!success) {
        const QString message = tr("无法%1当前用户的开机启动项。")
                .arg(enabled ? tr("写入") : tr("删除"));
        if (errorMessage)
            *errorMessage = message;
        emit updateFailed(message);
        return false;
    }

    if (wasEnabled != enabled)
        emit enabledChanged(enabled);
    return true;
#else
    const QString message = tr("当前平台不支持 Windows 开机启动项。");
    if (errorMessage)
        *errorMessage = message;
    emit updateFailed(message);
    return false;
#endif
}
