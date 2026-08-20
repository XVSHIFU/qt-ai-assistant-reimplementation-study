#include "windowcontroller.h"

#include <QCloseEvent>
#include <QCoreApplication>
#include <QCursor>
#include <QDesktopServices>
#include <QEvent>
#include <QGuiApplication>
#include <QQuickView>
#include <QScreen>
#include <QTimer>
#include <QUrl>

#ifdef Q_OS_WIN
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#endif

WindowController::WindowController(QQuickView *view, QObject *parent)
    : QObject(parent), m_view(view)
{
    Q_ASSERT(view);
    if (!m_view)
        return;

    applyQtWindowFlags();
    m_view->installEventFilter(this);
    connect(m_view, &QQuickView::xChanged, this, &WindowController::geometryChanged);
    connect(m_view, &QQuickView::yChanged, this, &WindowController::geometryChanged);
    connect(m_view, &QQuickView::widthChanged, this, &WindowController::geometryChanged);
    connect(m_view, &QQuickView::heightChanged, this, &WindowController::geometryChanged);
    if (!m_view->isVisible())
        setState(BackgroundHidden);
}

WindowController::~WindowController()
{
    if (m_view)
        m_view->removeEventFilter(this);
}

QQuickView *WindowController::view() const
{
    return m_view.data();
}

bool WindowController::isVisible() const
{
    return m_view && m_view->isVisible();
}

qreal WindowController::x() const
{
    return m_view ? m_view->x() : 0.0;
}

qreal WindowController::y() const
{
    return m_view ? m_view->y() : 0.0;
}

qreal WindowController::width() const
{
    return m_view ? m_view->width() : 0.0;
}

qreal WindowController::height() const
{
    return m_view ? m_view->height() : 0.0;
}

QString WindowController::newVersionNumber() const
{
    const QString version = QCoreApplication::applicationVersion();
    return version.isEmpty() ? QStringLiteral("1.0.0") : version;
}

QRect WindowController::clampedGeometry(const QRect &requested, const QRect &availableGeometry)
{
    if (!availableGeometry.isValid())
        return QRect(requested.topLeft(), requested.size().expandedTo(QSize(1, 1)));

    const int width = qBound(1, requested.width(), availableGeometry.width());
    const int height = qBound(1, requested.height(), availableGeometry.height());
    const int maximumX = availableGeometry.x() + availableGeometry.width() - width;
    const int maximumY = availableGeometry.y() + availableGeometry.height() - height;
    const int x = qBound(availableGeometry.x(), requested.x(), maximumX);
    const int y = qBound(availableGeometry.y(), requested.y(), maximumY);
    return QRect(x, y, width, height);
}

QRectF WindowController::availableGeometry() const
{
    QScreen *screen = m_view ? m_view->screen() : nullptr;
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    return screen ? QRectF(screen->availableGeometry()) : QRectF();
}

void WindowController::applyQtWindowFlags()
{
    if (!m_view)
        return;

    Qt::WindowFlags flags = m_view->flags();
    flags &= ~Qt::WindowType_Mask;
    flags |= Qt::Tool;
    flags |= Qt::FramelessWindowHint;
    flags |= Qt::NoDropShadowWindowHint;
    if (m_alwaysOnTop)
        flags |= Qt::WindowStaysOnTopHint;
    else
        flags &= ~Qt::WindowStaysOnTopHint;
    m_view->setFlags(flags);
}

void WindowController::enforceNoTaskbarStyle()
{
    if (!m_view)
        return;

#ifdef Q_OS_WIN
    const HWND hwnd = reinterpret_cast<HWND>(m_view->winId());
    if (!hwnd)
        return;

    LONG_PTR extendedStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    extendedStyle |= WS_EX_TOOLWINDOW;
    extendedStyle &= ~WS_EX_APPWINDOW;
    SetWindowLongPtrW(hwnd, GWL_EXSTYLE, extendedStyle);
    SetWindowPos(hwnd,
                 m_alwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST,
                 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
#else
    applyQtWindowFlags();
#endif
}

void WindowController::ensureReachableGeometry()
{
    if (!m_view)
        return;

    const QRect windowRect(m_view->position(), m_view->size());
    const auto screens = QGuiApplication::screens();
    QScreen *bestScreen = nullptr;
    int bestIntersection = 0;
    for (QScreen *screen : screens) {
        if (!screen)
            continue;
        const QRect intersection = screen->availableGeometry().intersected(windowRect);
        const int area = intersection.width() * intersection.height();
        if (area > bestIntersection) {
            bestIntersection = area;
            bestScreen = screen;
        }
    }
    QScreen *screen = bestScreen ? bestScreen : m_view->screen();
    if (!screen)
        screen = QGuiApplication::screenAt(QCursor::pos());
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;
    m_view->setGeometry(clampedGeometry(windowRect, screen->availableGeometry()));
}

void WindowController::positionInitialWindow()
{
    if (!m_view || m_initialPositionApplied)
        return;

    QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;

    // availableGeometry already excludes a taskbar docked to any edge. Keep a
    // small breathing space above it and center the compact assistant window.
    const QRect available = screen->availableGeometry();
    const int margin = 20; // QWindow coordinates and availableGeometry are logical pixels.
    const int x = available.x() + (available.width() - m_view->width()) / 2;
    const int y = available.y() + available.height() - m_view->height() - margin;
    m_view->setPosition(qMax(available.left(), x), qMax(available.top(), y));
    m_initialPositionApplied = true;
}

void WindowController::reveal()
{
    if (!m_view || m_exitInProgress)
        return;

    positionInitialWindow();
    ensureReachableGeometry();
    m_view->show();
    enforceNoTaskbarStyle();
    m_view->raise();
    m_view->requestActivate();
    activateNativeWindow();
    setState(m_view->isActive() ? VisibleActive : VisibleInactive);

    QTimer::singleShot(0, this, [this] {
        if (!m_view || m_exitInProgress)
            return;
        enforceNoTaskbarStyle();
        m_view->raise();
        m_view->requestActivate();
        activateNativeWindow();
        emit inputFocusRequested();
        emit revealed();
    });
}

void WindowController::hideToTray()
{
    if (!m_view || m_exitInProgress)
        return;
    m_view->hide();
    setState(BackgroundHidden);
    emit hiddenToTray();
}

void WindowController::toggleFromTray()
{
    if (isVisible())
        hideToTray();
    else
        reveal();
}

void WindowController::toggleFromHotkey()
{
    // A global shortcut is a true toggle: first press reveals and focuses the
    // chat window, the next press hides it back to the notification area.
    toggleFromTray();
}

void WindowController::setAlwaysOnTop(bool enabled)
{
    if (m_alwaysOnTop == enabled)
        return;
    m_alwaysOnTop = enabled;

#ifdef Q_OS_WIN
    enforceNoTaskbarStyle();
#else
    const bool wasVisible = isVisible();
    applyQtWindowFlags();
    if (wasVisible)
        m_view->show();
#endif
    emit alwaysOnTopChanged(m_alwaysOnTop);
}

void WindowController::setX(qreal value)
{
    if (m_view)
        m_view->setX(qRound(value));
}

void WindowController::setY(qreal value)
{
    if (m_view)
        m_view->setY(qRound(value));
}

void WindowController::setWidth(qreal value)
{
    if (m_view)
        m_view->setWidth(qMax(1, qRound(value)));
}

void WindowController::setHeight(qreal value)
{
    if (m_view)
        m_view->setHeight(qMax(1, qRound(value)));
}

void WindowController::setAgreePolicy(bool value)
{
    if (m_agreePolicy == value)
        return;
    m_agreePolicy = value;
    emit agreePolicyChanged(m_agreePolicy);
}

void WindowController::setSkinMode(int value)
{
    if (m_skinMode == value)
        return;
    m_skinMode = value;
    emit skinModeChanged(m_skinMode);
}

void WindowController::openWebsite(const QString &url)
{
    emit openWebsiteRequested(url);
    const QUrl target = QUrl::fromUserInput(url);
    if (target.isValid())
        QDesktopServices::openUrl(target);
}

void WindowController::setDialog(bool open)
{
    emit dialogStateRequested(open);
}

void WindowController::setFirstStart(bool firstStart)
{
    emit firstStartRequested(firstStart);
}

void WindowController::setHistory(bool open)
{
    emit historyStateRequested(open);
}

void WindowController::setXandY(qreal xValue, qreal yValue, qreal widthValue, qreal heightValue)
{
    if (!m_view)
        return;
    const QRect requested(qRound(xValue), qRound(yValue),
                          qMax(1, qRound(widthValue)), qMax(1, qRound(heightValue)));
    QScreen *screen = QGuiApplication::screenAt(requested.center());
    if (!screen)
        screen = m_view->screen();
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    m_view->setGeometry(screen ? clampedGeometry(requested, screen->availableGeometry())
                               : requested);
}

bool WindowController::beginSystemMove()
{
    if (!m_view || !m_view->isVisible())
        return false;
#ifdef Q_OS_WIN
    const HWND hwnd = reinterpret_cast<HWND>(m_view->winId());
    if (!hwnd)
        return false;
    ReleaseCapture();
    SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
    return true;
#else
    return false;
#endif
}

bool WindowController::beginSystemResize(int edgeValue)
{
    if (!m_view || !m_view->isVisible())
        return false;

    const Qt::Edges edges = Qt::Edges(edgeValue);
    const Qt::Edges knownEdges = Qt::LeftEdge | Qt::TopEdge
            | Qt::RightEdge | Qt::BottomEdge;
    if (edges == Qt::Edges() || (edges & ~knownEdges) != Qt::Edges()
            || ((edges & Qt::LeftEdge) && (edges & Qt::RightEdge))
            || ((edges & Qt::TopEdge) && (edges & Qt::BottomEdge))) {
        return false;
    }

#ifdef Q_OS_WIN
    int hitTest = 0;
    if (edges == (Qt::LeftEdge | Qt::TopEdge))
        hitTest = HTTOPLEFT;
    else if (edges == (Qt::RightEdge | Qt::TopEdge))
        hitTest = HTTOPRIGHT;
    else if (edges == (Qt::LeftEdge | Qt::BottomEdge))
        hitTest = HTBOTTOMLEFT;
    else if (edges == (Qt::RightEdge | Qt::BottomEdge))
        hitTest = HTBOTTOMRIGHT;
    else if (edges == Qt::LeftEdge)
        hitTest = HTLEFT;
    else if (edges == Qt::TopEdge)
        hitTest = HTTOP;
    else if (edges == Qt::RightEdge)
        hitTest = HTRIGHT;
    else if (edges == Qt::BottomEdge)
        hitTest = HTBOTTOM;
    if (!hitTest)
        return false;

    const HWND hwnd = reinterpret_cast<HWND>(m_view->winId());
    if (!hwnd)
        return false;
    ReleaseCapture();
    SendMessageW(hwnd, WM_NCLBUTTONDOWN, static_cast<WPARAM>(hitTest), 0);
    return true;
#else
    return m_view->startSystemResize(edges);
#endif
}

void WindowController::showGuideWindow()
{
    emit guideWindowRequested();
}

void WindowController::updateSoft()
{
    emit updateRequested();
}

void WindowController::beginExit()
{
    if (m_exitInProgress)
        return;
    m_exitInProgress = true;
    setState(Exiting);
    if (m_view)
        m_view->close();
}

void WindowController::setState(State state)
{
    if (m_state == state)
        return;
    m_state = state;
    emit stateChanged(m_state);
}

void WindowController::activateNativeWindow()
{
#ifdef Q_OS_WIN
    if (!m_view)
        return;
    const HWND hwnd = reinterpret_cast<HWND>(m_view->winId());
    if (hwnd)
        SetForegroundWindow(hwnd);
#endif
}

bool WindowController::eventFilter(QObject *watched, QEvent *event)
{
    if (watched != m_view || !event)
        return QObject::eventFilter(watched, event);

    switch (event->type()) {
    case QEvent::Close:
        if (!m_exitInProgress) {
            static_cast<QCloseEvent *>(event)->ignore();
            hideToTray();
            return true;
        }
        break;
    case QEvent::Show:
        QTimer::singleShot(0, this, &WindowController::enforceNoTaskbarStyle);
        setState(m_view->isActive() ? VisibleActive : VisibleInactive);
        break;
    case QEvent::Hide:
        if (!m_exitInProgress)
            setState(BackgroundHidden);
        break;
    case QEvent::WindowActivate:
        if (!m_exitInProgress)
            setState(VisibleActive);
        break;
    case QEvent::WindowDeactivate:
        if (!m_exitInProgress && m_view->isVisible())
            setState(VisibleInactive);
        break;
    default:
        break;
    }

    return QObject::eventFilter(watched, event);
}
