#pragma once

#include "aitypes.h"

#include <functional>

namespace smartkey {
namespace ai {

struct TokenBudgetResult
{
    QList<AiMessage> messages;
    qint64 estimatedInputTokens = 0;
    int outputLimit = 0;
    int omittedMessages = 0;
    int omittedTurns = 0;
    bool truncated = false;
    bool fits = true;
};

// Applies a deterministic message budget. The estimator is injectable so a
// provider-specific tokenizer can replace the conservative fallback later.
class TokenBudget final
{
public:
    using Estimator = std::function<qint64(const AiMessage &message)>;

    explicit TokenBudget(Estimator estimator = Estimator());

    TokenBudgetResult fit(const QList<AiMessage> &messages,
                          int contextLimit, int outputLimit) const;

    static qint64 conservativeEstimate(const AiMessage &message);

private:
    Estimator m_estimator;
};

} // namespace ai
} // namespace smartkey
