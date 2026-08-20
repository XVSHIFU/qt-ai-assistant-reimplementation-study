#include "diagnostics_exporter.h"

#include "storage/chat_storage.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSysInfo>

namespace smartkey {
namespace {

QString allowListedToken(const QVariant &value, const QStringList &allowed)
{
    const QString text = value.toString().trimmed();
    return allowed.contains(text) ? text : QStringLiteral("[REDACTED]");
}

QVariantMap restrictedLogRecord(const QVariantMap &source)
{
    QVariantMap record;
    const QDateTime timestamp = QDateTime::fromString(
        source.value(QStringLiteral("timestamp")).toString(), Qt::ISODate);
    if (timestamp.isValid())
        record.insert(QStringLiteral("timestamp"), timestamp.toUTC().toString(Qt::ISODateWithMs));
    record.insert(QStringLiteral("level"), allowListedToken(
        source.value(QStringLiteral("level")),
        {QStringLiteral("debug"), QStringLiteral("info"), QStringLiteral("warning"),
         QStringLiteral("error"), QStringLiteral("critical")}));
    record.insert(QStringLiteral("category"), allowListedToken(
        source.value(QStringLiteral("category")),
        {QStringLiteral("application"), QStringLiteral("storage"),
         QStringLiteral("settings"), QStringLiteral("ai.transport"),
         QStringLiteral("ai.models"), QStringLiteral("privacy")}));
    record.insert(QStringLiteral("event"), allowListedToken(
        source.value(QStringLiteral("event")),
        {QStringLiteral("started"), QStringLiteral("shutdown_flush"),
         QStringLiteral("open_failed"), QStringLiteral("checkpoint_failed"),
         QStringLiteral("legacy_import"), QStringLiteral("operation_failed"),
         QStringLiteral("request_failed"), QStringLiteral("request_completed"),
         QStringLiteral("discovery_failed"), QStringLiteral("data_deletion_requested")}));

    const QVariantMap fields = source.value(QStringLiteral("fields")).toMap();
    static const QStringList numericOrBoolean = {
        QStringLiteral("http_status"), QStringLiteral("native_code"),
        QStringLiteral("conversations"), QStringLiteral("messages"),
        QStringLiteral("success"), QStringLiteral("already_imported"),
        QStringLiteral("background")
    };
    QVariantMap safeFields;
    for (const QString &key : numericOrBoolean) {
        const QVariant value = fields.value(key);
        if (value.type() == QVariant::Bool || value.canConvert<qint64>())
            safeFields.insert(key, value.type() == QVariant::Bool ? value : value.toLongLong());
    }
    if (!safeFields.isEmpty())
        record.insert(QStringLiteral("fields"), safeFields);
    return record;
}

QVariantList restrictedLogs(const QString &logFilePath)
{
    QVariantList logs;
    QStringList files{logFilePath};
    for (int index = 1; index <= 20; ++index)
        files.append(logFilePath + QLatin1Char('.') + QString::number(index));
    for (const QString &path : files) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            continue;
        while (!file.atEnd() && logs.size() < 500) {
            const QByteArray line = file.readLine(64 * 1024);
            if (line.size() >= 64 * 1024)
                continue;
            QJsonParseError error;
            const QJsonDocument document = QJsonDocument::fromJson(line, &error);
            if (error.error == QJsonParseError::NoError && document.isObject())
                logs.append(restrictedLogRecord(document.object().toVariantMap()));
        }
        if (logs.size() >= 500)
            break;
    }
    return logs;
}

} // namespace

bool DiagnosticsExporter::exportPackage(const ChatStorage &storage,
                                        const QString &logFilePath,
                                        const QString &applicationVersion,
                                        const QString &directoryPath,
                                        QString *outputPath, QString *errorMessage)
{
    QDir directory(QFileInfo(directoryPath).absoluteFilePath());
    if (!directory.exists() && !QDir().mkpath(directory.absolutePath())) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Unable to create diagnostics directory");
        return false;
    }
    const QString fileName = QStringLiteral("smartkey-diagnostics-%1.json")
        .arg(QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd-HHmmss")));
    const QString path = directory.absoluteFilePath(fileName);
    if (QFileInfo(path).absolutePath().compare(directory.absolutePath(), Qt::CaseInsensitive) != 0) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Unsafe diagnostics path");
        return false;
    }

    const QVariantMap root{
        {QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("generatedAt"),
         QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {QStringLiteral("scope"), QStringLiteral(
         "version,environment,redacted_logs,database_status_and_counts")},
        {QStringLiteral("environment"), QVariantMap{
            {QStringLiteral("applicationVersion"), applicationVersion},
            {QStringLiteral("qtVersion"), QString::fromLatin1(qVersion())},
            {QStringLiteral("os"), QSysInfo::productType()},
            {QStringLiteral("osVersion"), QSysInfo::productVersion()},
            {QStringLiteral("architecture"), QSysInfo::currentCpuArchitecture()}
        }},
        {QStringLiteral("database"), storage.statistics()},
        {QStringLiteral("logs"), restrictedLogs(logFilePath)}
    };
    const QByteArray payload = QJsonDocument::fromVariant(root).toJson(QJsonDocument::Indented);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(payload) != payload.size()
            || !file.commit()) {
        file.cancelWriting();
        if (errorMessage)
            *errorMessage = QStringLiteral("Unable to write diagnostics package");
        return false;
    }
    if (outputPath)
        *outputPath = path;
    if (errorMessage)
        errorMessage->clear();
    return true;
}

} // namespace smartkey
