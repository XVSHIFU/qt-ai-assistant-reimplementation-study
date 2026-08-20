#pragma once

#include "aitypes.h"

#include <QObject>

namespace smartkey {
namespace ai {

// Isolation boundary only. It deliberately contains no endpoint, credential,
// fallback, or network behavior.
class LegacyRapooProvider final : public QObject
{
    Q_OBJECT
public:
    explicit LegacyRapooProvider(QObject *parent = nullptr) : QObject(parent) {}
    bool isEnabled() const { return false; }
    QString start(const AiRequest &request);
    void cancel(const QString &requestId);

signals:
    void contentDelta(const QString &requestId, const QString &text);
    void reasoningDelta(const QString &requestId, const QString &text);
    void referenceDelta(const QString &requestId, const QString &text);
    void usage(const QString &requestId, const QVariantMap &usageData);
    void completed(const QString &requestId);
    void failed(const QString &requestId, const smartkey::ai::AiError &error);
};

} // namespace ai
} // namespace smartkey

