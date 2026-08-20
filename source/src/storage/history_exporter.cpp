#include "history_exporter.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTextStream>

namespace smartkey {
namespace {

const int MaximumExportedConversations = 10000;

QString markdownText(const QVariantList &conversations)
{
    QString output;
    QTextStream stream(&output);
    stream.setCodec("UTF-8");
    for (int index = 0; index < conversations.size(); ++index) {
        const QVariantMap conversation = conversations.at(index).toMap();
        stream << "# " << conversation.value(QStringLiteral("title")).toString() << "\n\n";
        const QString model = conversation.value(QStringLiteral("model")).toString();
        if (!model.isEmpty())
            stream << "- Model: " << model << "\n";
        stream << "- Updated: " << conversation.value(QStringLiteral("updatedAt")).toString()
               << "\n\n";
        const QVariantList messages = conversation.value(QStringLiteral("messages")).toList();
        for (const QVariant &value : messages) {
            const QVariantMap message = value.toMap();
            const QString role = message.value(QStringLiteral("role")).toString();
            stream << "## " << (role == QLatin1String("user") ? "User" : "Assistant") << "\n\n";
            stream << message.value(QStringLiteral("content")).toString() << "\n\n";
            const QString reasoning = message.value(QStringLiteral("reasoningContent")).toString();
            if (!reasoning.isEmpty())
                stream << "> Reasoning\n> " << QString(reasoning).replace('\n', "\n> ") << "\n\n";
            const QString reference = message.value(QStringLiteral("reference")).toString();
            if (!reference.isEmpty())
                stream << "> Reference: " << reference << "\n\n";
        }
        if (index + 1 < conversations.size())
            stream << "---\n\n";
    }
    return output;
}

QVariantMap exportConversationMap(const ChatStorage &storage, const QVariantMap &source)
{
    QVariantMap result{
        {QStringLiteral("id"), source.value(QStringLiteral("id"))},
        {QStringLiteral("title"), source.value(QStringLiteral("title"))},
        {QStringLiteral("model"), source.value(QStringLiteral("model"))},
        {QStringLiteral("createdAt"), source.value(QStringLiteral("createdAt"))},
        {QStringLiteral("updatedAt"), source.value(QStringLiteral("updatedAt"))}
    };
    QVariantList safeMessages;
    const QVariantList messages = storage.messages(source.value(QStringLiteral("id")).toString());
    safeMessages.reserve(messages.size());
    for (const QVariant &value : messages) {
        const QVariantMap message = value.toMap();
        safeMessages.append(QVariantMap{
            {QStringLiteral("role"), message.value(QStringLiteral("role"))},
            {QStringLiteral("content"), message.value(QStringLiteral("content"))},
            {QStringLiteral("reasoningContent"), message.value(QStringLiteral("reasoningContent"))},
            {QStringLiteral("reference"), message.value(QStringLiteral("reference"))},
            {QStringLiteral("status"), message.value(QStringLiteral("status"))},
            {QStringLiteral("createdAt"), message.value(QStringLiteral("createdAt"))}
        });
    }
    result.insert(QStringLiteral("messages"), safeMessages);
    return result;
}

} // namespace

QString HistoryExporter::safeFileName(const QString &requestedName)
{
    QString name = requestedName.trimmed();
    name.replace(QRegularExpression(QStringLiteral("[<>:\"/\\\\|?*\\x00-\\x1F]")),
                 QStringLiteral("_"));
    name.replace(QRegularExpression(QStringLiteral("[. ]+$")), QString());
    if (name.size() > 96)
        name = name.left(96).trimmed();
    if (name.isEmpty())
        name = QStringLiteral("chat-history");
    static const QRegularExpression reserved(
        QStringLiteral("^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\\..*)?$"),
        QRegularExpression::CaseInsensitiveOption);
    if (reserved.match(name).hasMatch())
        name.prepend(QLatin1Char('_'));
    return name;
}

bool HistoryExporter::exportConversation(const ChatStorage &storage,
                                         const QString &conversationId,
                                         const QString &directoryPath,
                                         const QString &requestedName, Format format,
                                         QString *outputPath, QString *errorMessage)
{
    const QVariantMap conversation = storage.conversation(conversationId, true);
    if (conversation.isEmpty()) {
        if (errorMessage)
            *errorMessage = storage.errorString();
        return false;
    }
    return write(storage, {conversation}, directoryPath, requestedName,
                 format, outputPath, errorMessage);
}

bool HistoryExporter::exportQuery(const ChatStorage &storage, const ConversationQuery &filter,
                                  const QString &directoryPath, const QString &requestedName,
                                  Format format, QString *outputPath, QString *errorMessage)
{
    ConversationQuery query = filter;
    query.limit = ChatStorage::MaximumHistoryPageSize;
    query.cursorPinned = -1;
    query.cursorUpdatedAt.clear();
    query.cursorId.clear();
    QVariantList conversations;
    for (;;) {
        const ConversationPage page = storage.queryConversations(query);
        if (page.items.isEmpty() && !storage.errorString().isEmpty()) {
            if (errorMessage)
                *errorMessage = storage.errorString();
            return false;
        }
        conversations.append(page.items);
        if (conversations.size() > MaximumExportedConversations) {
            if (errorMessage)
                *errorMessage = QStringLiteral("Export contains too many conversations");
            return false;
        }
        if (!page.hasMore)
            break;
        query.cursorPinned = page.nextCursorPinned;
        query.cursorUpdatedAt = page.nextCursorUpdatedAt;
        query.cursorId = page.nextCursorId;
    }
    return write(storage, conversations, directoryPath, requestedName,
                 format, outputPath, errorMessage);
}

bool HistoryExporter::write(const ChatStorage &storage, const QVariantList &conversations,
                            const QString &directoryPath, const QString &requestedName,
                            Format format, QString *outputPath, QString *errorMessage)
{
    QDir directory(QFileInfo(directoryPath).absoluteFilePath());
    if (!directory.exists() && !QDir().mkpath(directory.absolutePath())) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Unable to create export directory");
        return false;
    }
    const QString extension = format == Format::Json ? QStringLiteral(".json")
                                                      : QStringLiteral(".md");
    QString baseName = safeFileName(requestedName);
    if (baseName.endsWith(extension, Qt::CaseInsensitive))
        baseName.chop(extension.size());
    const QString path = directory.absoluteFilePath(baseName + extension);
    if (QFileInfo(path).absolutePath().compare(directory.absolutePath(), Qt::CaseInsensitive) != 0) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Unsafe export path");
        return false;
    }

    QVariantList safeConversations;
    safeConversations.reserve(conversations.size());
    for (const QVariant &value : conversations)
        safeConversations.append(exportConversationMap(storage, value.toMap()));

    QByteArray payload;
    if (format == Format::Json) {
        const QVariantMap root{
            {QStringLiteral("schemaVersion"), 1},
            {QStringLiteral("exportedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
            {QStringLiteral("conversations"), safeConversations}
        };
        payload = QJsonDocument::fromVariant(root).toJson(QJsonDocument::Indented);
    } else {
        payload = markdownText(safeConversations).toUtf8();
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(payload) != payload.size() || !file.commit()) {
        file.cancelWriting();
        if (errorMessage)
            *errorMessage = QStringLiteral("Unable to write export file");
        return false;
    }
    if (outputPath)
        *outputPath = path;
    if (errorMessage)
        errorMessage->clear();
    return true;
}

} // namespace smartkey
