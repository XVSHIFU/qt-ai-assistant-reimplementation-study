#pragma once

#include <QObject>
#include <QPointer>
#include <QRectF>
#include <QVariantMap>

class QQuickView;
class WindowController;

class WindowFacade final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(qreal x READ x WRITE setX NOTIFY geometryChanged)
    Q_PROPERTY(qreal y READ y WRITE setY NOTIFY geometryChanged)
    Q_PROPERTY(qreal width READ width WRITE setWidth NOTIFY geometryChanged)
    Q_PROPERTY(qreal height READ height WRITE setHeight NOTIFY geometryChanged)
    Q_PROPERTY(qreal scale READ scale CONSTANT)
    Q_PROPERTY(bool agreePolicy READ agreePolicy WRITE setAgreePolicy NOTIFY agreePolicyChanged)
    Q_PROPERTY(bool allowQuestion READ allowQuestion WRITE setAllowQuestion NOTIFY allowQuestionChanged)
    Q_PROPERTY(bool notEnough READ notEnough CONSTANT)
    Q_PROPERTY(QString newVersionNumber READ newVersionNumber CONSTANT)
    Q_PROPERTY(bool topScreen READ topScreen WRITE setTopScreen NOTIFY topScreenChanged)
    Q_PROPERTY(qreal radius READ radius CONSTANT)
    Q_PROPERTY(int skinMode READ skinMode WRITE setSkinMode NOTIFY skinModeChanged)
    Q_PROPERTY(int sysSkinType READ sysSkinType CONSTANT)

public:
    WindowFacade(QQuickView *view, WindowController *controller, QObject *parent = nullptr);

    qreal x() const;
    qreal y() const;
    qreal width() const;
    qreal height() const;
    qreal scale() const { return 1.0; }
    bool agreePolicy() const { return m_agreePolicy; }
    bool allowQuestion() const { return m_allowQuestion; }
    bool notEnough() const { return false; }
    QString newVersionNumber() const;
    bool topScreen() const;
    qreal radius() const { return 12.0; }
    int skinMode() const { return m_skinMode; }
    int sysSkinType() const { return 0; }

    void setX(qreal value);
    void setY(qreal value);
    void setWidth(qreal value);
    void setHeight(qreal value);
    void setAgreePolicy(bool value);
    void setAllowQuestion(bool value);
    void setTopScreen(bool value);
    void setSkinMode(int value);

    Q_INVOKABLE void openWebsite(const QString &url);
    Q_INVOKABLE void setDialog(bool open);
    Q_INVOKABLE void setFirstStart(bool firstStart);
    Q_INVOKABLE void setHistory(bool open);
    Q_INVOKABLE void setXandY(qreal x, qreal y, qreal width, qreal height);
    Q_INVOKABLE QRectF availableGeometry() const;
    Q_INVOKABLE bool beginSystemMove();
    Q_INVOKABLE bool beginSystemResize(int edges);
    Q_INVOKABLE QVariantMap savedMainWindowSize() const;
    Q_INVOKABLE void saveMainWindowSize(qreal width, qreal height);
    Q_INVOKABLE void setMinimumSize(qreal width, qreal height);
    Q_INVOKABLE void showGuideWindow();
    Q_INVOKABLE void updateSoft();
    Q_INVOKABLE void revealAndActivate();
    Q_INVOKABLE void hideMain();
    Q_INVOKABLE void exitApplication();

signals:
    void geometryChanged();
    void agreePolicyChanged();
    void allowQuestionChanged();
    void topScreenChanged();
    void skinModeChanged();
    void settingsRequested();
    void updateRequested();
    void exitRequested();

private:
    QPointer<QQuickView> m_view;
    WindowController *m_controller = nullptr;
    bool m_agreePolicy = false;
    bool m_allowQuestion = false;
    int m_skinMode = 0;
};
