#pragma once

#include <QObject>
#include <QPointer>
#include <QRect>
#include <QString>

class QEvent;
class QQuickView;

class WindowController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool alwaysOnTop READ alwaysOnTop WRITE setAlwaysOnTop NOTIFY alwaysOnTopChanged)
    Q_PROPERTY(qreal x READ x WRITE setX NOTIFY geometryChanged)
    Q_PROPERTY(qreal y READ y WRITE setY NOTIFY geometryChanged)
    Q_PROPERTY(qreal width READ width WRITE setWidth NOTIFY geometryChanged)
    Q_PROPERTY(qreal height READ height WRITE setHeight NOTIFY geometryChanged)
    Q_PROPERTY(qreal scale READ scale CONSTANT)
    Q_PROPERTY(bool agreePolicy READ agreePolicy WRITE setAgreePolicy NOTIFY agreePolicyChanged)
    Q_PROPERTY(bool allowQuestion READ allowQuestion CONSTANT)
    Q_PROPERTY(bool notEnough READ notEnough CONSTANT)
    Q_PROPERTY(QString newVersionNumber READ newVersionNumber NOTIFY newVersionNumberChanged)
    Q_PROPERTY(bool topScreen READ alwaysOnTop WRITE setAlwaysOnTop NOTIFY alwaysOnTopChanged)
    Q_PROPERTY(qreal radius READ radius CONSTANT)
    Q_PROPERTY(int skinMode READ skinMode WRITE setSkinMode NOTIFY skinModeChanged)
    Q_PROPERTY(int sysSkinType READ sysSkinType CONSTANT)

public:
    enum State {
        BackgroundHidden,
        VisibleInactive,
        VisibleActive,
        Exiting
    };
    Q_ENUM(State)

    explicit WindowController(QQuickView *view, QObject *parent = nullptr);
    ~WindowController() override;

    QQuickView *view() const;
    State state() const { return m_state; }
    bool alwaysOnTop() const { return m_alwaysOnTop; }
    bool isVisible() const;
    qreal x() const;
    qreal y() const;
    qreal width() const;
    qreal height() const;
    qreal scale() const { return 1.0; }
    bool agreePolicy() const { return m_agreePolicy; }
    bool allowQuestion() const { return true; }
    bool notEnough() const { return false; }
    QString newVersionNumber() const;
    qreal radius() const { return 12.0; }
    int skinMode() const { return m_skinMode; }
    int sysSkinType() const { return 0; }

    // Pure logical-pixel geometry helper. availableGeometry may start at a
    // negative coordinate on monitors placed left/above the primary display.
    static QRect clampedGeometry(const QRect &requested, const QRect &availableGeometry);
    Q_INVOKABLE QRectF availableGeometry() const;

public slots:
    void reveal();
    void hideToTray();
    void toggleFromTray();
    void toggleFromHotkey();
    void setAlwaysOnTop(bool enabled);
    void setX(qreal value);
    void setY(qreal value);
    void setWidth(qreal value);
    void setHeight(qreal value);
    void setAgreePolicy(bool value);
    void setSkinMode(int value);
    void beginExit();
    void enforceNoTaskbarStyle();

    Q_INVOKABLE void openWebsite(const QString &url);
    Q_INVOKABLE void setDialog(bool open);
    Q_INVOKABLE void setFirstStart(bool firstStart);
    Q_INVOKABLE void setHistory(bool open);
    Q_INVOKABLE void setXandY(qreal x, qreal y, qreal width, qreal height);
    Q_INVOKABLE bool beginSystemMove();
    Q_INVOKABLE bool beginSystemResize(int edges);
    Q_INVOKABLE void showGuideWindow();
    Q_INVOKABLE void updateSoft();

signals:
    void stateChanged(WindowController::State state);
    void alwaysOnTopChanged(bool enabled);
    void inputFocusRequested();
    void revealed();
    void hiddenToTray();
    void geometryChanged();
    void agreePolicyChanged(bool agreed);
    void newVersionNumberChanged(const QString &version);
    void skinModeChanged(int mode);
    void openWebsiteRequested(const QString &url);
    void dialogStateRequested(bool open);
    void firstStartRequested(bool firstStart);
    void historyStateRequested(bool open);
    void guideWindowRequested();
    void updateRequested();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void applyQtWindowFlags();
    void positionInitialWindow();
    void ensureReachableGeometry();
    void setState(State state);
    void activateNativeWindow();

    QPointer<QQuickView> m_view;
    State m_state = BackgroundHidden;
    bool m_alwaysOnTop = false;
    bool m_exitInProgress = false;
    bool m_initialPositionApplied = false;
    bool m_agreePolicy = true;
    int m_skinMode = 0;
};
