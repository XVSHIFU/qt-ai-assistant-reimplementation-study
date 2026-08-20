#pragma once

#include <QMutex>
#include <QSet>
#include <QString>
#include <QVariantMap>

namespace smartkey {

// Local-only structured logging. Values are recursively scrubbed before they
// reach disk; no transport or upload behavior is part of this class.
class JsonlLogger final
{
public:
    explicit JsonlLogger(const QString &filePath, qint64 maxBytes = 2 * 1024 * 1024,
                         int backupCount = 5);

    QString filePath() const { return m_filePath; }
    QString errorString() const;

    void addSensitiveValue(const QString &value);
    void clearSensitiveValues();

    bool write(const QString &category, const QString &event,
               const QVariantMap &fields = QVariantMap(),
               const QString &level = QStringLiteral("info"));

    static QVariant redact(const QVariant &value, const QSet<QString> &knownSecrets = {});

private:
    static bool isSensitiveKey(const QString &key);
    static QString scrubString(QString value, const QSet<QString> &knownSecrets);
    bool rotateIfNeeded(qint64 incomingBytes);
    void setError(const QString &error);

    QString m_filePath;
    qint64 m_maxBytes;
    int m_backupCount;
    mutable QMutex m_mutex;
    QSet<QString> m_knownSecrets;
    QString m_errorString;
};

} // namespace smartkey
