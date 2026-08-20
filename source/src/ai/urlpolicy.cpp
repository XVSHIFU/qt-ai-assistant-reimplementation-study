#include "urlpolicy.h"

namespace smartkey {
namespace ai {

namespace {

UrlPolicyResult rejected(AiErrorCode code, const QString &message)
{
    UrlPolicyResult result;
    result.error.code = code;
    result.error.message = message;
    return result;
}

} // namespace

bool UrlPolicy::isLoopbackHost(const QString &host)
{
    const QString normalized = host.trimmed().toLower();
    if (normalized == QStringLiteral("localhost") || normalized.endsWith(QStringLiteral(".localhost")))
        return true;
    if (normalized == QStringLiteral("::1"))
        return true;

    const QStringList parts = normalized.split(QLatin1Char('.'));
    if (parts.size() != 4)
        return false;
    for (const QString &part : parts) {
        bool ok = false;
        const uint value = part.toUInt(&ok, 10);
        if (!ok || value > 255 || (part.size() > 1 && part.startsWith(QLatin1Char('0'))))
            return false;
    }
    return parts.first() == QStringLiteral("127");
}

UrlPolicyResult UrlPolicy::validateEndpoint(const QUrl &input, bool allowLoopbackHttp)
{
    if (!input.isValid() || input.isRelative() || input.host().isEmpty())
        return rejected(AiErrorCode::InvalidUrl, QStringLiteral("The API endpoint must be an absolute URL with a host."));

    if (!input.userInfo().isEmpty())
        return rejected(AiErrorCode::InvalidUrl, QStringLiteral("Credentials in the API URL are not allowed."));

    if (input.hasFragment())
        return rejected(AiErrorCode::InvalidUrl, QStringLiteral("Fragments are not allowed in an API endpoint."));

    const QString scheme = input.scheme().toLower();
    if (scheme != QStringLiteral("https") && scheme != QStringLiteral("http"))
        return rejected(AiErrorCode::UnsupportedScheme, QStringLiteral("Only HTTPS endpoints are supported."));

    if (scheme == QStringLiteral("http") && !(allowLoopbackHttp && isLoopbackHost(input.host())))
        return rejected(AiErrorCode::InsecureUrl, QStringLiteral("HTTP is allowed only for an explicitly enabled loopback endpoint."));

    if (input.port() == 0 || input.port() > 65535)
        return rejected(AiErrorCode::InvalidUrl, QStringLiteral("The API endpoint contains an invalid port."));

    QUrl canonical = input.adjusted(QUrl::NormalizePathSegments | QUrl::StripTrailingSlash);
    canonical.setScheme(scheme);
    canonical.setHost(input.host().toLower());
    canonical.setUserInfo(QString());
    canonical.setFragment(QString());
    if (canonical.path().isEmpty())
        canonical.setPath(QStringLiteral("/"));

    UrlPolicyResult result;
    result.accepted = true;
    result.canonicalUrl = canonical;
    return result;
}

} // namespace ai
} // namespace smartkey
