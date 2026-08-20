#pragma once

#include <QColor>
#include <QObject>
#include <QTranslator>

class QQmlEngine;

class UiPreferences final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString themeMode READ themeMode WRITE setThemeMode NOTIFY appearanceChanged)
    Q_PROPERTY(QString resolvedTheme READ resolvedTheme NOTIFY appearanceChanged)
    Q_PROPERTY(bool highContrast READ highContrast NOTIFY appearanceChanged)
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(QString resolvedLanguage READ resolvedLanguage NOTIFY languageChanged)

    Q_PROPERTY(QColor backgroundColor READ backgroundColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor surfaceColor READ surfaceColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor elevatedColor READ elevatedColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor textPrimaryColor READ textPrimaryColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor textSecondaryColor READ textSecondaryColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor placeholderColor READ placeholderColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor borderColor READ borderColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor accentColor READ accentColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor accentHoverColor READ accentHoverColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor focusRingColor READ focusRingColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor disabledSurfaceColor READ disabledSurfaceColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor disabledTextColor READ disabledTextColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor successColor READ successColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor warningColor READ warningColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor dangerColor READ dangerColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor errorSurfaceColor READ errorSurfaceColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor shadowColor READ shadowColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor overlayColor READ overlayColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor selectionColor READ selectionColor NOTIFY appearanceChanged)

    Q_PROPERTY(int spacingXs READ spacingXs CONSTANT)
    Q_PROPERTY(int spacingSm READ spacingSm CONSTANT)
    Q_PROPERTY(int spacingMd READ spacingMd CONSTANT)
    Q_PROPERTY(int spacingLg READ spacingLg CONSTANT)
    Q_PROPERTY(int radiusSm READ radiusSm CONSTANT)
    Q_PROPERTY(int radiusMd READ radiusMd CONSTANT)
    Q_PROPERTY(int radiusLg READ radiusLg CONSTANT)
    Q_PROPERTY(int fontSmall READ fontSmall CONSTANT)
    Q_PROPERTY(int fontBody READ fontBody CONSTANT)
    Q_PROPERTY(int fontTitle READ fontTitle CONSTANT)
    Q_PROPERTY(int touchTarget READ touchTarget CONSTANT)

public:
    explicit UiPreferences(QObject *parent = nullptr);

    QString themeMode() const { return m_themeMode; }
    QString resolvedTheme() const;
    bool highContrast() const { return false; }
    QString language() const { return m_language; }
    QString resolvedLanguage() const;

    QColor backgroundColor() const;
    QColor surfaceColor() const;
    QColor elevatedColor() const;
    QColor textPrimaryColor() const;
    QColor textSecondaryColor() const;
    QColor placeholderColor() const;
    QColor borderColor() const;
    QColor accentColor() const;
    QColor accentHoverColor() const;
    QColor focusRingColor() const;
    QColor disabledSurfaceColor() const;
    QColor disabledTextColor() const;
    QColor successColor() const;
    QColor warningColor() const;
    QColor dangerColor() const;
    QColor errorSurfaceColor() const;
    QColor shadowColor() const;
    QColor overlayColor() const;
    QColor selectionColor() const;

    int spacingXs() const { return 4; }
    int spacingSm() const { return 8; }
    int spacingMd() const { return 12; }
    int spacingLg() const { return 16; }
    int radiusSm() const { return 8; }
    int radiusMd() const { return 12; }
    int radiusLg() const { return 18; }
    int fontSmall() const { return 11; }
    int fontBody() const { return 13; }
    int fontTitle() const { return 18; }
    int touchTarget() const { return 40; }

    void setEngine(QQmlEngine *engine);
    Q_INVOKABLE void setThemeMode(const QString &mode);
    Q_INVOKABLE void setLanguage(const QString &language);
    Q_INVOKABLE QString themeSource() const;

signals:
    void appearanceChanged();
    void languageChanged();

private:
    void applyLanguage();

    QString m_themeMode;
    QString m_language;
    QTranslator m_translator;
    QQmlEngine *m_engine = nullptr;
};
