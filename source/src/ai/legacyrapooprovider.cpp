#include "legacyrapooprovider.h"

#include <QTimer>
#include <QUuid>

namespace smartkey {
namespace ai {

QString LegacyRapooProvider::start(const AiRequest &request)
{
    Q_UNUSED(request)
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    AiError error;
    error.code = AiErrorCode::LegacyDisabled;
    error.message = QStringLiteral("The legacy vendor service is disabled in this build.");
    QTimer::singleShot(0, this, [this, id, error] { emit failed(id, error); });
    return id;
}

void LegacyRapooProvider::cancel(const QString &requestId)
{
    Q_UNUSED(requestId)
}

} // namespace ai
} // namespace smartkey

