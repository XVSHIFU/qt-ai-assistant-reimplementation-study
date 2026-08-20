#pragma once

#include "chat_storage.h"

#include <QString>

namespace smartkey {

class HistoryExporter final
{
public:
    enum class Format { Markdown, Json };

    static QString safeFileName(const QString &requestedName);
    static bool exportConversation(const ChatStorage &storage, const QString &conversationId,
                                   const QString &directoryPath, const QString &requestedName,
                                   Format format, QString *outputPath, QString *errorMessage);
    static bool exportQuery(const ChatStorage &storage, const ConversationQuery &query,
                            const QString &directoryPath, const QString &requestedName,
                            Format format, QString *outputPath, QString *errorMessage);

private:
    static bool write(const ChatStorage &storage, const QVariantList &conversations,
                      const QString &directoryPath, const QString &requestedName,
                      Format format, QString *outputPath, QString *errorMessage);
};

} // namespace smartkey
