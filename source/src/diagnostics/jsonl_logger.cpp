#include "jsonl_logger.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace smartkey {
namespace {

const QString kRedacted = QStringLiteral("[REDACTED]");

QString normalizedKey(QString key)
{
    key.remove(QRegularExpression(QStringLiteral("[^A-Za-z0-9]")));
    return key.toLower();
}

} // namespace

JsonlLogger::JsonlLogger(const QString &filePath, qint64 maxBytes, int backupCount)
    : m_filePath(QFileInfo(filePath).absoluteFilePath()),
      m_maxBytes(qMax<qint64>(1024, maxBytes)),
      m_backupCount(qBound(1, backupCount, 20))
{
}

QString JsonlLogger::errorString() const
{
    QMutexLocker locker(&m_mutex);
    return m_errorString;
}

void JsonlLogger::addSensitiveValue(const QString &value)
{
    if (value.size() < 4)
        return;
    QMutexLocker locker(&m_mutex);
    m_knownSecrets.insert(value);
}

void JsonlLogger::clearSensitiveValues()
{
    QMutexLocker locker(&m_mutex);
    m_knownSecrets.clear();
}

bool JsonlLogger::write(const QString &category, const QString &event,
                        const QVariantMap &fields, const QString &level)
{
    QMutexLocker locker(&m_mutex);

    QVariantMap record;
    record.insert(QStringLiteral("timestamp"),
                  QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    record.insert(QStringLiteral("level"), scrubString(level, m_knownSecrets));
    record.insert(QStringLiteral("category"), scrubString(category, m_knownSecrets));
    record.insert(QStringLiteral("event"), scrubString(event, m_knownSecrets));
    record.insert(QStringLiteral("fields"), redact(fields, m_knownSecrets));

    QByteArray line = QJsonDocument::fromVariant(record).toJson(QJsonDocument::Compact);
    line.append('\n');
    if (!rotateIfNeeded(line.size()))
        return false;

    const QFileInfo info(m_filePath);
    if (!QDir().mkpath(info.absolutePath())) {
        setError(QStringLiteral("Could not create log directory: %1").arg(info.absolutePath()));
        return false;
    }

    QFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        setError(QStringLiteral("Could not open log file: %1").arg(file.errorString()));
        return false;
    }
    if (file.write(line) != line.size() || !file.flush()) {
        setError(QStringLiteral("Could not write log file: %1").arg(file.errorString()));
        return false;
    }
    m_errorString.clear();
    return true;
}

QVariant JsonlLogger::redact(const QVariant &value, const QSet<QString> &knownSecrets)
{
    if (value.type() == QVariant::Map) {
        QVariantMap output;
        const QVariantMap input = value.toMap();
        for (auto it = input.constBegin(); it != input.constEnd(); ++it) {
            output.insert(it.key(), isSensitiveKey(it.key())
                                      ? QVariant(kRedacted)
                                      : redact(it.value(), knownSecrets));
        }
        return output;
    }
    if (value.type() == QVariant::Hash) {
        QVariantMap output;
        const QVariantHash input = value.toHash();
        for (auto it = input.constBegin(); it != input.constEnd(); ++it) {
            output.insert(it.key(), isSensitiveKey(it.key())
                                      ? QVariant(kRedacted)
                                      : redact(it.value(), knownSecrets));
        }
        return output;
    }
    if (value.type() == QVariant::List || value.type() == QVariant::StringList) {
        QVariantList output;
        const QVariantList input = value.toList();
        output.reserve(input.size());
        for (const QVariant &item : input)
            output.append(redact(item, knownSecrets));
        return output;
    }
    if (value.type() == QVariant::String)
        return scrubString(value.toString(), knownSecrets);
    if (value.type() == QVariant::ByteArray)
        return scrubString(QString::fromUtf8(value.toByteArray()), knownSecrets);
    return value;
}

bool JsonlLogger::isSensitiveKey(const QString &key)
{
    const QString normalized = normalizedKey(key);
    static const QStringList exact = {
        QStringLiteral("apikey"), QStringLiteral("authorization"),
        QStringLiteral("authheader"), QStringLiteral("authvalue"),
        QStringLiteral("bearertoken"), QStringLiteral("cookie"),
        QStringLiteral("credential"), QStringLiteral("password"),
        QStringLiteral("secret"), QStringLiteral("token"),
        QStringLiteral("prompt"), QStringLiteral("response"),
        QStringLiteral("requestbody"), QStringLiteral("responsebody"),
        QStringLiteral("headers"), QStringLiteral("messages"),
        QStringLiteral("input"), QStringLiteral("output"),
        QStringLiteral("content"), QStringLiteral("reasoningcontent")
    };
    if (exact.contains(normalized))
        return true;
    return normalized.endsWith(QStringLiteral("apikey"))
        || normalized.endsWith(QStringLiteral("token"))
        || normalized.endsWith(QStringLiteral("password"))
        || normalized.endsWith(QStringLiteral("secret"))
        || normalized.endsWith(QStringLiteral("credential"));
}

QString JsonlLogger::scrubString(QString value, const QSet<QString> &knownSecrets)
{
    for (const QString &secret : knownSecrets) {
        if (secret.size() >= 4)
            value.replace(secret, kRedacted, Qt::CaseSensitive);
    }

    static const QRegularExpression authorization(
        QStringLiteral("(?i)\\b(authorization\\s*[:=]\\s*)(?:bearer|basic)\\s+[^\\s,;]+"));
    value.replace(authorization, QStringLiteral("\\1") + kRedacted);

    static const QRegularExpression bearer(
        QStringLiteral("(?i)\\b(bearer|basic)\\s+[A-Za-z0-9._~+/=-]{8,}"));
    value.replace(bearer, kRedacted);

    static const QRegularExpression commonApiKey(
        QStringLiteral("\\b(?:sk|rk|pk|sess)-[A-Za-z0-9_-]{8,}\\b"),
        QRegularExpression::CaseInsensitiveOption);
    value.replace(commonApiKey, kRedacted);

    static const QRegularExpression jsonSecret(
        QStringLiteral("(?i)(\\\"(?:api[_-]?key|authorization|token|password|secret)\\\"\\s*:\\s*\\\")[^\\\"]*(\\\")"));
    value.replace(jsonSecret, QStringLiteral("\\1") + kRedacted + QStringLiteral("\\2"));

    static const QRegularExpression querySecret(
        QStringLiteral("(?i)([?&](?:api[_-]?key|access[_-]?token|token|key)=)[^&#\\s]+"));
    value.replace(querySecret, QStringLiteral("\\1") + kRedacted);
    return value;
}

bool JsonlLogger::rotateIfNeeded(qint64 incomingBytes)
{
    const QFileInfo info(m_filePath);
    if (!info.exists() || info.size() + incomingBytes <= m_maxBytes)
        return true;

    const QString oldest = m_filePath + QLatin1Char('.') + QString::number(m_backupCount);
    if (QFile::exists(oldest) && !QFile::remove(oldest)) {
        setError(QStringLiteral("Could not remove oldest log backup"));
        return false;
    }
    for (int index = m_backupCount - 1; index >= 1; --index) {
        const QString source = m_filePath + QLatin1Char('.') + QString::number(index);
        const QString target = m_filePath + QLatin1Char('.') + QString::number(index + 1);
        if (QFile::exists(source) && !QFile::rename(source, target)) {
            setError(QStringLiteral("Could not rotate log backup %1").arg(index));
            return false;
        }
    }
    if (QFile::exists(m_filePath) && !QFile::rename(m_filePath, m_filePath + QStringLiteral(".1"))) {
        setError(QStringLiteral("Could not rotate active log file"));
        return false;
    }
    return true;
}

void JsonlLogger::setError(const QString &error)
{
    m_errorString = error;
}

} // namespace smartkey
