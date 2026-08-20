#pragma once

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QString>

namespace smartkey {

enum class StartupAction
{
    RunSmoke,
    ShowManualUi,
    ShowSettings,
    RevealMain,
    HideToTray
};

inline StartupAction decideStartupAction(bool smokeTest, bool manualUiTest,
                                         bool background, bool trayAvailable,
                                         bool configured, bool startupCompleted)
{
    if (smokeTest)
        return StartupAction::RunSmoke;
    if (manualUiTest)
        return StartupAction::ShowManualUi;
    // Onboarding must remain reachable even for an accidental --background
    // launch before the first provider has been configured.
    if (!configured || !startupCompleted)
        return StartupAction::ShowSettings;
    if (background && trayAvailable)
        return StartupAction::HideToTray;
    // A normal launch is always user-visible. If the tray is unavailable, a
    // requested background launch also falls back to a visible window.
    return StartupAction::RevealMain;
}

inline bool isSafeSmokeDataRoot(const QString &candidate, QString *canonicalRoot = nullptr)
{
    const QFileInfo candidateInfo(candidate);
    const QString resolvedCandidate = candidateInfo.canonicalFilePath();
    const QString resolvedTemp = QFileInfo(QDir::tempPath()).canonicalFilePath();
    if (!candidateInfo.isAbsolute() || !candidateInfo.exists() || !candidateInfo.isDir()
            || resolvedCandidate.isEmpty() || resolvedTemp.isEmpty())
        return false;

    static const QRegularExpression safeName(
        QStringLiteral("^SmartKeyAI-smoke-[A-Za-z0-9_-]+-[0-9A-Fa-f]{32}$"));
    if (!safeName.match(QFileInfo(resolvedCandidate).fileName()).hasMatch())
        return false;

    QString tempPrefix = QDir::fromNativeSeparators(QDir::cleanPath(resolvedTemp));
    if (!tempPrefix.endsWith(QLatin1Char('/')))
        tempPrefix.append(QLatin1Char('/'));
    const QString cleanCandidate = QDir::fromNativeSeparators(
                QDir::cleanPath(resolvedCandidate));
#ifdef Q_OS_WIN
    const Qt::CaseSensitivity pathCase = Qt::CaseInsensitive;
#else
    const Qt::CaseSensitivity pathCase = Qt::CaseSensitive;
#endif
    if (!cleanCandidate.startsWith(tempPrefix, pathCase))
        return false;
    if (canonicalRoot)
        *canonicalRoot = cleanCandidate;
    return true;
}

} // namespace smartkey
