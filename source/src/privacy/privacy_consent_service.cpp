#include "privacy_consent_service.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QSettings>

namespace smartkey {
namespace {

const char kAcceptedKey[] = "privacy/accepted";
const char kPolicyVersionKey[] = "privacy/policyVersion";
const char kAcceptedAtKey[] = "privacy/acceptedAt";
const char kLanguageKey[] = "privacy/language";

} // namespace

PrivacyConsentService::PrivacyConsentService(const QString &settingsFilePath,
                                             const QString &policyVersion,
                                             QObject *parent)
    : QObject(parent),
      m_settingsFilePath(settingsFilePath),
      m_policyVersion(policyVersion.trimmed())
{
    if (m_policyVersion.isEmpty())
        m_policyVersion = currentPolicyVersion();
    load();
}

QString PrivacyConsentService::currentPolicyVersion()
{
    return QStringLiteral("2026.08.13.1");
}

QString PrivacyConsentService::policyText() const
{
    return tr(
        "智键 AI 隐私说明（版本 %1）\n\n"
        "1. 第三方 AI Provider\n"
        "当你发送消息、测试连接或获取模型列表时，本软件会把相应请求发送到你在设置中选择的第三方 Provider。聊天请求通常包含当前提示词、为保持上下文所需的历史消息、所选模型及推理/搜索选项。第三方如何处理数据由其自身条款和隐私政策决定。\n\n"
        "2. 本地数据\n"
        "聊天记录以明文形式保存在当前 Windows 用户的本地 SQLite 数据库中；API Key 使用 Windows DPAPI 加密保存。请保护好 Windows 账户和设备。\n\n"
        "3. 日志\n"
        "本地诊断日志可能记录时间、错误类别、HTTP 状态和请求标识，但设计上不记录 API Key、提示词或回答正文。诊断日志不会由本软件自动上传。\n\n"
        "4. 管理与删除\n"
        "你可以在设置中撤回同意。撤回后所有第三方网络请求会被阻止。你也可以请求清除本地数据；清除前软件必须再次确认，且本说明页面本身不会自动删除任何内容。\n\n"
        "继续即表示你已阅读并同意以上说明。政策版本更新后，软件会要求重新确认。")
        .arg(m_policyVersion);
}

bool PrivacyConsentService::accept(const QString &language)
{
    const QString normalizedLanguage = language.trimmed().isEmpty()
            ? QLocale::system().name().replace(QLatin1Char('_'), QLatin1Char('-'))
            : language.trimmed();
    const QString timestamp = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    if (!persist(true, timestamp, normalizedLanguage))
        return false;
    m_consentGranted = true;
    m_acceptedAt = timestamp;
    m_acceptedLanguage = normalizedLanguage;
    setLastError(QString());
    emit consentChanged();
    return true;
}

bool PrivacyConsentService::revoke()
{
    if (!persist(false, QString(), m_acceptedLanguage))
        return false;
    const bool changed = m_consentGranted || !m_acceptedAt.isEmpty();
    m_consentGranted = false;
    m_acceptedAt.clear();
    setLastError(QString());
    if (changed)
        emit consentChanged();
    return true;
}

bool PrivacyConsentService::requireConsent(const QString &action)
{
    if (m_consentGranted)
        return true;
    emit consentRequiredForAction(action.trimmed().isEmpty()
                                  ? QStringLiteral("network") : action.trimmed());
    return false;
}

void PrivacyConsentService::requestPolicyDisplay()
{
    emit policyDisplayRequested();
}

void PrivacyConsentService::requestDataDeletion()
{
    emit dataDeletionRequested();
}

bool PrivacyConsentService::persist(bool accepted, const QString &acceptedAt,
                                   const QString &language)
{
    if (!QDir().mkpath(QFileInfo(m_settingsFilePath).absolutePath())) {
        setLastError(tr("无法创建隐私设置目录。"));
        return false;
    }
    QSettings settings(m_settingsFilePath, QSettings::IniFormat);
    settings.setValue(QLatin1String(kAcceptedKey), accepted);
    settings.setValue(QLatin1String(kPolicyVersionKey), m_policyVersion);
    settings.setValue(QLatin1String(kAcceptedAtKey), acceptedAt);
    settings.setValue(QLatin1String(kLanguageKey), language);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        setLastError(tr("无法保存隐私同意状态。"));
        return false;
    }
    return true;
}

void PrivacyConsentService::setLastError(const QString &message)
{
    if (m_lastError == message)
        return;
    m_lastError = message;
    emit lastErrorChanged();
}

void PrivacyConsentService::load()
{
    QSettings settings(m_settingsFilePath, QSettings::IniFormat);
    const bool accepted = settings.value(QLatin1String(kAcceptedKey), false).toBool();
    const QString storedVersion = settings.value(QLatin1String(kPolicyVersionKey)).toString();
    const QString storedAt = settings.value(QLatin1String(kAcceptedAtKey)).toString();
    const QString storedLanguage = settings.value(QLatin1String(kLanguageKey)).toString();
    const QDateTime acceptedTime = QDateTime::fromString(storedAt, Qt::ISODateWithMs);
    m_consentGranted = accepted && storedVersion == m_policyVersion
            && acceptedTime.isValid() && !storedLanguage.trimmed().isEmpty();
    if (m_consentGranted) {
        m_acceptedAt = storedAt;
        m_acceptedLanguage = storedLanguage;
    } else {
        m_acceptedAt.clear();
        m_acceptedLanguage = storedLanguage;
    }
}

} // namespace smartkey
