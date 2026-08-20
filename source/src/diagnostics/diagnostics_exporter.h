#pragma once

#include <QString>

namespace smartkey {

class ChatStorage;

class DiagnosticsExporter final
{
public:
    static bool exportPackage(const ChatStorage &storage, const QString &logFilePath,
                              const QString &applicationVersion,
                              const QString &directoryPath, QString *outputPath,
                              QString *errorMessage);
};

} // namespace smartkey
