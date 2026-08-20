#pragma once

#include "aitypes.h"

namespace smartkey {
namespace ai {

struct UrlPolicyResult {
    bool accepted = false;
    QUrl canonicalUrl;
    AiError error;
};

class UrlPolicy final
{
public:
    static UrlPolicyResult validateEndpoint(const QUrl &url, bool allowLoopbackHttp);
    static bool isLoopbackHost(const QString &host);
};

} // namespace ai
} // namespace smartkey

