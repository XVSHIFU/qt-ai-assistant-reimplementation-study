#include "data_management_service.h"

#include "diagnostics/diagnostics_exporter.h"
#include "settings/provider_settings.h"
#include "storage/chat_storage.h"
#include "storage/history_exporter.h"

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QUrl>

namespace smartkey {

DataManagementService::DataManagementService(ChatStorage *storage,
                                             ProviderSettings *providerSettings,
                                             const QString &settingsFilePath,
                                             const QString &dataDirectory,
                                             const QString &logFilePath,
                                             const QString &applicationVersion,
                                             QObject *parent)
    : QObject(parent), m_storage(storage), m_providerSettings(providerSettings),
      m_settingsFilePath(QFileInfo(settingsFilePath).absoluteFilePath()),
      m_dataDirectory(QFileInfo(dataDirectory).absoluteFilePath()),
      m_logFilePath(QFileInfo(logFilePath).absoluteFilePath()),
      m_applicationVersion(applicationVersion)
{
    QSettings settings(m_settingsFilePath, QSettings::IniFormat);
    m_historyPersistenceEnabled = settings.value(
        QStringLiteral("history/persistenceEnabled"), true).toBool();
    const int retention = settings.value(QStringLiteral("history/retentionDays"), 0).toInt();
    m_retentionDays = retention == 7 || retention == 30 || retention == 90 ? retention : 0;
    if (m_storage)
        m_storage->setHistoryPersistenceEnabled(m_historyPersistenceEnabled);
    refreshStatistics();
}

void DataManagementService::refreshStatistics()
{
    const QVariantMap statistics = m_storage ? m_storage->statistics() : QVariantMap();
    m_activeChatCount = statistics.value(QStringLiteral("activeConversations")).toInt();
    m_deletedChatCount = statistics.value(QStringLiteral("deletedConversations")).toInt();
    m_databaseBytes = databaseFileBytes();
    m_logBytes = logFileBytes();
    emit statisticsChanged();
}

bool DataManagementService::setHistoryPersistenceEnabled(bool enabled)
{
    if (m_historyPersistenceEnabled == enabled)
        return true;
    const bool previous = m_historyPersistenceEnabled;
    m_historyPersistenceEnabled = enabled;
    if (!persistPolicy()) {
        m_historyPersistenceEnabled = previous;
        setOperation(false, tr("无法保存历史记录策略，原设置已保留。"));
        return false;
    }
    if (m_storage)
        m_storage->setHistoryPersistenceEnabled(enabled);
    emit policyChanged();
    setOperation(true, enabled
                 ? tr("新消息将继续保存到本地。")
                 : tr("已开启“不保存历史”；为避免静默丢失输入，发送将被阻止。"));
    return true;
}

bool DataManagementService::setRetentionDays(int days)
{
    if (days != 0 && days != 7 && days != 30 && days != 90) {
        setOperation(false, tr("不支持的保留期限。"));
        return false;
    }
    if (m_retentionDays == days)
        return true;
    const int previous = m_retentionDays;
    m_retentionDays = days;
    if (!persistPolicy()) {
        m_retentionDays = previous;
        setOperation(false, tr("无法保存保留期限，原设置已保留。"));
        return false;
    }
    emit policyChanged();
    setOperation(true, days == 0 ? tr("聊天将永久保留，直到你手动删除。")
                                 : tr("已保存 %1 天保留期限；不会自动删除，需确认后应用。").arg(days));
    return true;
}

bool DataManagementService::applyRetentionPolicy()
{
    if (!m_storage || !m_storage->isOpen()) {
        setOperation(false, tr("聊天数据库不可用。"));
        return false;
    }
    if (m_retentionDays == 0) {
        setOperation(true, tr("永久保留策略无需清理。"));
        return true;
    }
    const int moved = m_storage->softDeleteOlderThan(m_retentionDays);
    if (moved < 0) {
        setOperation(false, tr("应用保留期限失败，未静默删除任何输入。"));
        return false;
    }
    refreshStatistics();
    emit chatsChanged();
    setOperation(true, tr("已将 %1 个过期会话移到最近删除。").arg(moved));
    return true;
}

bool DataManagementService::exportAllChats(const QString &directoryUrl,
                                           const QString &format)
{
    if (!m_storage || !m_storage->isOpen()) {
        setOperation(false, tr("聊天数据库不可用。"));
        return false;
    }
    const bool json = format.compare(QStringLiteral("json"), Qt::CaseInsensitive) == 0;
    if (!json && format.compare(QStringLiteral("markdown"), Qt::CaseInsensitive) != 0) {
        setOperation(false, tr("不支持的导出格式。"));
        return false;
    }
    ConversationQuery query;
    query.includeDeleted = true;
    QString outputPath;
    QString error;
    const bool ok = HistoryExporter::exportQuery(
        *m_storage, query, localDirectory(directoryUrl), tr("全部聊天"),
        json ? HistoryExporter::Format::Json : HistoryExporter::Format::Markdown,
        &outputPath, &error);
    setOperation(ok, ok ? tr("聊天已导出到 %1").arg(outputPath) : error);
    return ok;
}

bool DataManagementService::exportDiagnostics(const QString &directoryUrl)
{
    if (!m_storage) {
        setOperation(false, tr("无法读取数据库状态。"));
        return false;
    }
    QString outputPath;
    QString error;
    const bool ok = DiagnosticsExporter::exportPackage(
        *m_storage, m_logFilePath, m_applicationVersion,
        localDirectory(directoryUrl), &outputPath, &error);
    setOperation(ok, ok ? tr("受限诊断包已导出到 %1").arg(outputPath) : error);
    return ok;
}

bool DataManagementService::openDataDirectory()
{
    if (!QDir().mkpath(m_dataDirectory)) {
        setOperation(false, tr("无法创建数据目录。"));
        return false;
    }
    const bool ok = QDesktopServices::openUrl(QUrl::fromLocalFile(m_dataDirectory));
    setOperation(ok, ok ? tr("已打开数据目录。") : tr("系统无法打开数据目录。"));
    return ok;
}

bool DataManagementService::clearLogs()
{
    bool ok = true;
    QStringList files{m_logFilePath};
    for (int index = 1; index <= 20; ++index)
        files.append(m_logFilePath + QLatin1Char('.') + QString::number(index));
    for (const QString &path : files) {
        if (QFile::exists(path) && !QFile::remove(path))
            ok = false;
    }
    refreshStatistics();
    setOperation(ok, ok ? tr("本地日志已清除。") : tr("部分日志无法清除，请关闭占用文件的程序后重试。"));
    return ok;
}

bool DataManagementService::clearAllChats()
{
    if (!m_storage || !m_storage->clearAllConversations()) {
        setOperation(false, tr("清除聊天失败；数据库未完成事务提交。"));
        return false;
    }
    refreshStatistics();
    emit chatsChanged();
    setOperation(true, tr("全部聊天、最近删除和请求元数据已清除。"));
    return true;
}

bool DataManagementService::clearAllCredentials()
{
    QString error;
    const bool ok = m_providerSettings
            && m_providerSettings->clearAllCredentials(&error);
    setOperation(ok, ok ? tr("所有已保存凭据均已清除。")
                        : (error.isEmpty() ? tr("清除凭据失败。") : error));
    return ok;
}

void DataManagementService::setOperation(bool succeeded, const QString &message)
{
    m_operationSucceeded = succeeded;
    m_operationMessage = message;
    emit operationChanged();
}

bool DataManagementService::persistPolicy()
{
    QDir().mkpath(QFileInfo(m_settingsFilePath).absolutePath());
    QSettings settings(m_settingsFilePath, QSettings::IniFormat);
    settings.setValue(QStringLiteral("history/persistenceEnabled"),
                      m_historyPersistenceEnabled);
    settings.setValue(QStringLiteral("history/retentionDays"), m_retentionDays);
    settings.sync();
    return settings.status() == QSettings::NoError;
}

QString DataManagementService::localDirectory(const QString &pathOrUrl) const
{
    const QUrl url(pathOrUrl);
    return url.isLocalFile() ? url.toLocalFile() : pathOrUrl;
}

qint64 DataManagementService::databaseFileBytes() const
{
    if (!m_storage)
        return 0;
    const QString path = m_storage->databasePath();
    return QFileInfo(path).size() + QFileInfo(path + QStringLiteral("-wal")).size()
            + QFileInfo(path + QStringLiteral("-shm")).size();
}

qint64 DataManagementService::logFileBytes() const
{
    qint64 bytes = QFileInfo(m_logFilePath).size();
    for (int index = 1; index <= 20; ++index)
        bytes += QFileInfo(m_logFilePath + QLatin1Char('.') + QString::number(index)).size();
    return bytes;
}

} // namespace smartkey
