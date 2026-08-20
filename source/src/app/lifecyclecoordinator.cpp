#include "lifecyclecoordinator.h"

#include <utility>

namespace smartkey {

LifecycleCoordinator::LifecycleCoordinator(Hooks hooks, QObject *parent)
    : QObject(parent), m_hooks(std::move(hooks))
{
}

void LifecycleCoordinator::requestTrayExit()
{
    requestShutdown(Trigger::TrayExit);
}

void LifecycleCoordinator::requestQmlExit()
{
    requestShutdown(Trigger::QmlExit);
}

void LifecycleCoordinator::requestSystemExit()
{
    requestShutdown(Trigger::SystemExit);
}

void LifecycleCoordinator::requestShutdown(Trigger trigger)
{
    if (m_shutdownStarted)
        return;

    // Set the guard before invoking client code. quitApplication can cause an
    // immediate aboutToQuit re-entry on some event-loop paths.
    m_shutdownStarted = true;
    m_trigger = trigger;
    emit shutdownStarted(m_trigger);

    if (m_hooks.stopAcceptingRequests)
        m_hooks.stopAcceptingRequests();
    if (m_hooks.cancelRequests)
        m_hooks.cancelRequests();
    if (m_hooks.flushAndPersist)
        m_hooks.flushAndPersist();
    if (m_hooks.closeStorage)
        m_hooks.closeStorage();
    if (m_hooks.unregisterIntegrations)
        m_hooks.unregisterIntegrations();

    m_shutdownCompleted = true;
    emit shutdownCompleted(m_trigger);

    // aboutToQuit means the event loop is already leaving. Calling quit again
    // from that signal is unnecessary and risks recursive platform callbacks.
    if (m_trigger != Trigger::SystemExit && m_hooks.quitApplication)
        m_hooks.quitApplication();
}

} // namespace smartkey
