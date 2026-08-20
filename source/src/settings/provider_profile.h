#pragma once

#include <QString>
#include <QVariantMap>

namespace smartkey {

struct ProviderProfile
{
    static const int DefaultContextLimit = 32768;
    static const int DefaultOutputLimit = 2048;

    QString id;
    QString name;
    QString baseUrl;
    QString chatPath;
    QString model;
    QString providerType;
    QString apiStyle;
    QString authScheme;
    QString authHeaderName;
    QString capabilitySchema;
    QString searchRequestField;
    int timeoutMs = 60000;
    int contextLimit = DefaultContextLimit;
    int outputLimit = DefaultOutputLimit;
    bool supportsStreaming = true;
    bool supportsReasoning = false;
    bool supportsSearch = false;
    bool thinkingEnabled = false;
    QString reasoningEffort;

    static ProviderProfile fromVariantMap(const QVariantMap &map, const QString &id);
    QVariantMap toVariantMap(bool hasCredential) const;
    bool isValid(QString *errorMessage = nullptr) const;
    bool requiresCredential() const;
    bool hasUsableSearchCapability() const;
};

} // namespace smartkey
