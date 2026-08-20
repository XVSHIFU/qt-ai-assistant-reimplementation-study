#include "uipreferences.h"

#include <QCoreApplication>
#include <QLocale>
#include <QQmlEngine>
#include <QSettings>

namespace {

QString normalizedTheme(const QString &value)
{
    Q_UNUSED(value);
    return QStringLiteral("light");
}

QString normalizedLanguage(const QString &value)
{
    const QString language = value.trimmed();
    if (language.compare(QStringLiteral("zh_CN"), Qt::CaseInsensitive) == 0)
        return QStringLiteral("zh_CN");
    if (language.compare(QStringLiteral("en_US"), Qt::CaseInsensitive) == 0)
        return QStringLiteral("en_US");
    return QStringLiteral("system");
}

} // namespace

UiPreferences::UiPreferences(QObject *parent)
    : QObject(parent)
{
    QSettings settings;
    m_themeMode = normalizedTheme(settings.value(
                                      QStringLiteral("appearance/theme"),
                                      QStringLiteral("light")).toString());
    // Theme selection was removed. Migrate any existing system/dark/high-
    // contrast preference so every subsequent launch remains light.
    settings.setValue(QStringLiteral("appearance/theme"), QStringLiteral("light"));
    m_language = normalizedLanguage(settings.value(
                                      QStringLiteral("appearance/language"),
                                      QStringLiteral("system")).toString());
    applyLanguage();
}

QString UiPreferences::resolvedTheme() const
{
    return QStringLiteral("light");
}

QString UiPreferences::resolvedLanguage() const
{
    if (m_language != QStringLiteral("system"))
        return m_language;
    return QLocale::system().name().startsWith(QStringLiteral("zh"), Qt::CaseInsensitive)
            ? QStringLiteral("zh_CN") : QStringLiteral("en_US");
}

#define TOKEN_COLOR(name, light) \
QColor UiPreferences::name() const \
{ \
    return QColor(QStringLiteral(light)); \
}

TOKEN_COLOR(backgroundColor, "#F7F7F8")
TOKEN_COLOR(surfaceColor, "#FFFFFF")
TOKEN_COLOR(elevatedColor, "#FFFFFF")
TOKEN_COLOR(textPrimaryColor, "#18181B")
TOKEN_COLOR(textSecondaryColor, "#52525B")
TOKEN_COLOR(placeholderColor, "#6B7280")
TOKEN_COLOR(borderColor, "#D4D4D8")
TOKEN_COLOR(accentColor, "#4F46E5")
TOKEN_COLOR(accentHoverColor, "#3730A3")
TOKEN_COLOR(focusRingColor, "#3730A3")
TOKEN_COLOR(disabledSurfaceColor, "#E4E4E7")
TOKEN_COLOR(disabledTextColor, "#71717A")
TOKEN_COLOR(successColor, "#047857")
TOKEN_COLOR(warningColor, "#B45309")
TOKEN_COLOR(dangerColor, "#B91C1C")
TOKEN_COLOR(errorSurfaceColor, "#FEF2F2")
TOKEN_COLOR(shadowColor, "#00000000")
TOKEN_COLOR(overlayColor, "#52090B10")
TOKEN_COLOR(selectionColor, "#CACAFA")

#undef TOKEN_COLOR

void UiPreferences::setEngine(QQmlEngine *engine)
{
    m_engine = engine;
}

void UiPreferences::setThemeMode(const QString &mode)
{
    const QString normalized = normalizedTheme(mode);
    if (m_themeMode == normalized)
        return;
    m_themeMode = normalized;
    QSettings().setValue(QStringLiteral("appearance/theme"), m_themeMode);
    emit appearanceChanged();
}

void UiPreferences::setLanguage(const QString &language)
{
    const QString normalized = normalizedLanguage(language);
    if (m_language == normalized)
        return;
    m_language = normalized;
    QSettings().setValue(QStringLiteral("appearance/language"), m_language);
    applyLanguage();
    emit languageChanged();
}

QString UiPreferences::themeSource() const
{
    return QStringLiteral("qrc:/Theme/LightTheme.qml");
}

void UiPreferences::applyLanguage()
{
    QCoreApplication::removeTranslator(&m_translator);
    m_translator.load(QStringLiteral(":/i18n/smartkey_%1.qm").arg(resolvedLanguage()));
    QCoreApplication::installTranslator(&m_translator);
    QLocale::setDefault(QLocale(resolvedLanguage()));
    if (m_engine)
        m_engine->retranslate();
}
