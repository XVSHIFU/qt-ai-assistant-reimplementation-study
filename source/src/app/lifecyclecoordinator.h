#pragma once

#include <QObject>

#include <functional>

namespace smartkey {

class LifecycleCoordinator final : public QObject
{
    Q_OBJECT

public:
    enum class Trigger {
        TrayExit,
        QmlExit,
        SystemExit
    };
    Q_ENUM(Trigger)

    struct Hooks {
        std::function<void()> stopAcceptingRequests;
        std::function<void()> cancelRequests;
        std::function<void()> flushAndPersist;
        std::function<void()> closeStorage;
        std::function<void()> unregisterIntegrations;
        std::function<void()> quitApplication;
    };

    explicit LifecycleCoordinator(Hooks hooks, QObject *parent = nullptr);

    bool isShutdownStarted() const { return m_shutdownStarted; }
    bool isShutdownCompleted() const { return m_shutdownCompleted; }
    Trigger trigger() const { return m_trigger; }

public slots:
    void requestTrayExit();
    void requestQmlExit();
    void requestSystemExit();

signals:
    void shutdownStarted(smartkey::LifecycleCoordinator::Trigger trigger);
    void shutdownCompleted(smartkey::LifecycleCoordinator::Trigger trigger);

private:
    void requestShutdown(Trigger trigger);

    Hooks m_hooks;
    bool m_shutdownStarted = false;
    bool m_shutdownCompleted = false;
    Trigger m_trigger = Trigger::SystemExit;
};

} // namespace smartkey
