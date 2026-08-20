#include "tokenbudget.h"

#include <QVector>

#include <limits>
#include <utility>

namespace smartkey {
namespace ai {
namespace {

qint64 saturatingAdd(qint64 left, qint64 right)
{
    if (right > 0 && left > std::numeric_limits<qint64>::max() - right)
        return std::numeric_limits<qint64>::max();
    return left + right;
}

struct Turn
{
    QVector<int> indexes;
    qint64 tokens = 0;
};

} // namespace

TokenBudget::TokenBudget(Estimator estimator)
    : m_estimator(estimator ? std::move(estimator) : &TokenBudget::conservativeEstimate)
{
}

qint64 TokenBudget::conservativeEstimate(const AiMessage &message)
{
    // Three UTF-8 bytes per token plus fixed chat-envelope overhead errs on
    // the safe side for ordinary Latin and CJK prompts without pretending to
    // be a model-specific tokenizer.
    const qint64 bytes = message.role.toUtf8().size() + message.content.toUtf8().size();
    return 6 + ((bytes + 2) / 3);
}

TokenBudgetResult TokenBudget::fit(const QList<AiMessage> &messages,
                                   int contextLimit, int outputLimit) const
{
    TokenBudgetResult result;
    result.outputLimit = qMax(1, outputLimit);
    const qint64 inputBudget = qMax<qint64>(0,
                                            qint64(contextLimit) - result.outputLimit);

    QVector<qint64> estimates;
    estimates.reserve(messages.size());
    QVector<bool> keep(messages.size(), false);
    QVector<Turn> turns;
    qint64 systemTokens = 0;

    for (int index = 0; index < messages.size(); ++index) {
        const qint64 estimate = qMax<qint64>(1, m_estimator(messages.at(index)));
        estimates.append(estimate);
        const QString role = messages.at(index).role.trimmed().toLower();
        if (role == QLatin1String("system") || role == QLatin1String("developer")) {
            keep[index] = true;
            systemTokens = saturatingAdd(systemTokens, estimate);
            continue;
        }

        if (turns.isEmpty() || role == QLatin1String("user"))
            turns.append(Turn());
        turns.last().indexes.append(index);
        turns.last().tokens = saturatingAdd(turns.last().tokens, estimate);
    }

    result.estimatedInputTokens = systemTokens;
    if (systemTokens > inputBudget) {
        result.fits = false;
        result.truncated = !turns.isEmpty();
        result.omittedTurns = turns.size();
        for (const Turn &turn : turns)
            result.omittedMessages += turn.indexes.size();
        return result;
    }

    qint64 used = systemTokens;
    int firstRetainedTurn = turns.size();
    for (int turnIndex = turns.size() - 1; turnIndex >= 0; --turnIndex) {
        const qint64 turnTokens = turns.at(turnIndex).tokens;
        if (turnTokens > inputBudget - used) {
            // Never split a turn. If even the newest turn does not fit, the
            // caller must surface a context-limit error instead of sending a
            // partial prompt.
            if (turnIndex == turns.size() - 1)
                result.fits = false;
            break;
        }
        used += turnTokens;
        firstRetainedTurn = turnIndex;
    }

    if (!result.fits) {
        result.truncated = !turns.isEmpty();
        result.omittedTurns = turns.size();
        for (const Turn &turn : turns)
            result.omittedMessages += turn.indexes.size();
        result.estimatedInputTokens = used;
        return result;
    }

    for (int turnIndex = firstRetainedTurn; turnIndex < turns.size(); ++turnIndex) {
        for (int index : turns.at(turnIndex).indexes)
            keep[index] = true;
    }
    for (int turnIndex = 0; turnIndex < firstRetainedTurn; ++turnIndex) {
        ++result.omittedTurns;
        result.omittedMessages += turns.at(turnIndex).indexes.size();
    }

    result.truncated = result.omittedMessages > 0;
    result.estimatedInputTokens = used;
    result.messages.reserve(messages.size() - result.omittedMessages);
    for (int index = 0; index < messages.size(); ++index) {
        if (keep.at(index))
            result.messages.append(messages.at(index));
    }
    return result;
}

} // namespace ai
} // namespace smartkey
