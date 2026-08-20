#include "windowfacade.h"

#include "platform/windowcontroller.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QQuickView>
#include <QSettings>
#include <QSize>
#include <QUrl>
#include <QtMath>

WindowFacade::WindowFacade(QQuickView *view, WindowController *controller, QObject *parent)
    : QObject(parent), m_view(view), m_controller(controller)
{
    QSettings settings;
    // PrivacyConsentService is the sole persisted source of truth. This legacy
    // facade property remains only for recovered QML ABI compatibility.
    m_agreePolicy = false;
    m_skinMode = settings.value(QStringLiteral("appearance/skinMode"), 0).toInt();
    if (m_view) {
        connect(m_view, &QQuickView::xChanged, this, &WindowFacade::geometryChanged);
        connect(m_view, &QQuickView::yChanged, this, &WindowFacade::geometryChanged);
        connect(m_view, &QQuickView::widthChanged, this, &WindowFacade::geometryChanged);
        connect(m_view, &QQuickView::heightChanged, this, &WindowFacade::geometryChanged);
    }
    if (m_controller) {
        connect(m_controller, &WindowController::alwaysOnTopChanged,
                this, &WindowFacade::topScreenChanged);
    }
}

qreal WindowFacade::x() const { return m_view ? m_view->x() : 0; }
qreal WindowFacade::y() const { return m_view ? m_view->y() : 0; }
qreal WindowFacade::width() const { return m_view ? m_view->width() : 576; }
qreal WindowFacade::height() const { return m_view ? m_view->height() : 720; }
QString WindowFacade::newVersionNumber() const { return QCoreApplication::applicationVersion(); }
bool WindowFacade::topScreen() const { return m_controller && m_controller->alwaysOnTop(); }

void WindowFacade::setX(qreal value) { if (m_view) m_view->setX(qRound(value)); }
void WindowFacade::setY(qreal value) { if (m_view) m_view->setY(qRound(value)); }
void WindowFacade::setWidth(qreal value) { if (m_view) m_view->setWidth(qMax(1, qRound(value))); }
void WindowFacade::setHeight(qreal value) { if (m_view) m_view->setHeight(qMax(1, qRound(value))); }

void WindowFacade::setAgreePolicy(bool value)
{
    if (m_agreePolicy == value)
        return;
    m_agreePolicy = value;
    emit agreePolicyChanged();
}

void WindowFacade::setAllowQuestion(bool value)
{
    if (m_allowQuestion == value)
        return;
    m_allowQuestion = value;
    emit allowQuestionChanged();
}

void WindowFacade::setTopScreen(bool value)
{
    if (m_controller)
        m_controller->setAlwaysOnTop(value);
}

void WindowFacade::setSkinMode(int value)
{
    if (m_skinMode == value)
        return;
    m_skinMode = value;
    QSettings().setValue(QStringLiteral("appearance/skinMode"), value);
    emit skinModeChanged();
}

void WindowFacade::openWebsite(const QString &url)
{
    const QUrl parsed = QUrl::fromUserInput(url);
    if (parsed.scheme() == QStringLiteral("http") || parsed.scheme() == QStringLiteral("https"))
        QDesktopServices::openUrl(parsed);
}

void WindowFacade::setDialog(bool) {}
void WindowFacade::setFirstStart(bool firstStart) { QSettings().setValue(QStringLiteral("onboarding/completed"), !firstStart); }
void WindowFacade::setHistory(bool) {}

void WindowFacade::setXandY(qreal newX, qreal newY, qreal newWidth, qreal newHeight)
{
    if (m_controller)
        m_controller->setXandY(newX, newY, newWidth, newHeight);
}

QRectF WindowFacade::availableGeometry() const
{
    return m_controller ? m_controller->availableGeometry() : QRectF();
}

bool WindowFacade::beginSystemMove()
{
    return m_controller && m_controller->beginSystemMove();
}

bool WindowFacade::beginSystemResize(int edges)
{
    return m_controller && m_controller->beginSystemResize(edges);
}

QVariantMap WindowFacade::savedMainWindowSize() const
{
    QSettings settings;
    const qreal widthValue = settings.value(
                QStringLiteral("window/mainExpandedWidth"), 0.0).toReal();
    const qreal heightValue = settings.value(
                QStringLiteral("window/mainExpandedHeight"), 0.0).toReal();
    const bool valid = qIsFinite(widthValue) && qIsFinite(heightValue)
            && widthValue >= 480.0 && widthValue <= 10000.0
            && heightValue >= 440.0 && heightValue <= 10000.0;
    return QVariantMap {
        { QStringLiteral("valid"), valid },
        { QStringLiteral("width"), valid ? widthValue : 0.0 },
        { QStringLiteral("height"), valid ? heightValue : 0.0 }
    };
}

void WindowFacade::saveMainWindowSize(qreal widthValue, qreal heightValue)
{
    if (!qIsFinite(widthValue) || !qIsFinite(heightValue)
            || widthValue < 480.0 || widthValue > 10000.0
            || heightValue < 440.0 || heightValue > 10000.0) {
        return;
    }
    QSettings settings;
    settings.setValue(QStringLiteral("window/mainExpandedWidth"), widthValue);
    settings.setValue(QStringLiteral("window/mainExpandedHeight"), heightValue);
    settings.sync();
}

void WindowFacade::setMinimumSize(qreal widthValue, qreal heightValue)
{
    if (!m_view || !qIsFinite(widthValue) || !qIsFinite(heightValue))
        return;
    m_view->setMinimumSize(QSize(qMax(1, qRound(widthValue)),
                                 qMax(1, qRound(heightValue))));
}

void WindowFacade::showGuideWindow() { emit settingsRequested(); }
void WindowFacade::updateSoft() { emit updateRequested(); }
void WindowFacade::revealAndActivate() { if (m_controller) m_controller->reveal(); }
void WindowFacade::hideMain() { if (m_controller) m_controller->hideToTray(); }
void WindowFacade::exitApplication() { emit exitRequested(); }
