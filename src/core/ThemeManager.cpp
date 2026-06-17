#include "ThemeManager.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QDebug>

namespace {
constexpr auto DefaultThemePath = ":/themes/default/theme.json";

QString defaultThemePath()
{
    return QString::fromUtf8(DefaultThemePath);
}

QString selectedThemePath()
{
#ifdef RIP_SELECTED_THEME_RESOURCE
    return QStringLiteral(RIP_SELECTED_THEME_RESOURCE);
#else
    return QStringLiteral(DefaultThemePath);
#endif
}
}

ThemeManager::ThemeManager(QObject *parent)
    : QObject(parent)
{
}

bool ThemeManager::loadSelectedTheme()
{
    m_theme = QJsonObject();
    m_lastError.clear();

    const bool defaultLoaded = loadTheme(defaultThemePath(), true);
    if (!defaultLoaded) {
        qCritical() << "ThemeManager:" << m_lastError;
        return false;
    }

    const QString runtimeThemeFile = qEnvironmentVariable("RIP_THEME_FILE");
    const QString selectedPath = runtimeThemeFile.isEmpty() ? selectedThemePath() : runtimeThemeFile;
    if (selectedPath != defaultThemePath() && !loadTheme(selectedPath, true)) {
        qWarning() << "ThemeManager: falling back to default theme after load failure:" << m_lastError;
        m_lastError.clear();
        loadTheme(defaultThemePath(), true);
    }

    emit themeChanged();
    return true;
}

bool ThemeManager::loadFromPath(const QString &path)
{
    m_theme = QJsonObject();
    m_lastError.clear();
    if (!loadTheme(defaultThemePath(), true))
        return false;
    const bool loaded = loadTheme(path, true);
    emit themeChanged();
    return loaded;
}

QString ThemeManager::id() const { return stringValue(QStringLiteral("id")); }
QString ThemeManager::displayName() const { return stringValue(QStringLiteral("displayName")); }
QString ThemeManager::appName() const { return stringValue(QStringLiteral("appName")); }
QColor ThemeManager::primaryColor() const { return colorValue(QStringLiteral("primaryColor")); }
QColor ThemeManager::secondaryColor() const { return colorValue(QStringLiteral("secondaryColor")); }
QColor ThemeManager::backgroundColor() const { return colorValue(QStringLiteral("backgroundColor")); }
QColor ThemeManager::surfaceColor() const { return colorValue(QStringLiteral("surfaceColor")); }
QColor ThemeManager::surface2Color() const { return colorValue(QStringLiteral("surface2Color")); }
QColor ThemeManager::textColor() const { return colorValue(QStringLiteral("textColor")); }
QColor ThemeManager::subtextColor() const { return colorValue(QStringLiteral("subtextColor")); }
QColor ThemeManager::dividerColor() const { return colorValue(QStringLiteral("dividerColor")); }
QColor ThemeManager::lightBackgroundColor() const { return colorValue(QStringLiteral("lightBackgroundColor")); }
QColor ThemeManager::lightSurfaceColor() const { return colorValue(QStringLiteral("lightSurfaceColor")); }
QColor ThemeManager::lightSurface2Color() const { return colorValue(QStringLiteral("lightSurface2Color")); }
QColor ThemeManager::lightTextColor() const { return colorValue(QStringLiteral("lightTextColor")); }
QColor ThemeManager::lightSubtextColor() const { return colorValue(QStringLiteral("lightSubtextColor")); }
QColor ThemeManager::lightDividerColor() const { return colorValue(QStringLiteral("lightDividerColor")); }
QColor ThemeManager::accentColor() const { return colorValue(QStringLiteral("accentColor")); }
QString ThemeManager::logoPath() const { return qmlPath(stringValue(QStringLiteral("logoPath"))); }
QString ThemeManager::splashLogoPath() const { return qmlPath(stringValue(QStringLiteral("splashLogoPath"))); }
int ThemeManager::logoWidth() const { return intValue(QStringLiteral("logoWidth")); }
int ThemeManager::logoHeight() const { return intValue(QStringLiteral("logoHeight")); }
QString ThemeManager::aboutVendorName() const { return stringValue(QStringLiteral("aboutVendorName")); }
QString ThemeManager::supportUrl() const { return stringValue(QStringLiteral("supportUrl")); }
QString ThemeManager::copyrightText() const { return stringValue(QStringLiteral("copyrightText")); }
QString ThemeManager::sourcePath() const { return m_sourcePath; }
QString ThemeManager::lastError() const { return m_lastError; }

bool ThemeManager::loadTheme(const QString &path, bool requireIdentity)
{
    QJsonObject theme;
    if (!loadJsonObject(path, &theme))
        return false;

    return applyThemeObject(theme, path, requireIdentity);
}

bool ThemeManager::applyThemeObject(const QJsonObject &theme, const QString &path, bool requireIdentity)
{
    const QStringList required = {
        QStringLiteral("id"),
        QStringLiteral("displayName"),
        QStringLiteral("appName"),
    };

    if (requireIdentity) {
        for (const QString &key : required) {
            if (!theme.value(key).isString() || theme.value(key).toString().trimmed().isEmpty()) {
                m_lastError = QStringLiteral("Theme %1 is missing required string field '%2'.").arg(path, key);
                return false;
            }
        }
    }

    for (auto it = theme.constBegin(); it != theme.constEnd(); ++it)
        m_theme.insert(it.key(), it.value());

    m_sourcePath = path;
    return true;
}

bool ThemeManager::loadJsonObject(const QString &path, QJsonObject *theme)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        m_lastError = QStringLiteral("Unable to open theme file %1: %2").arg(path, file.errorString());
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        m_lastError = QStringLiteral("Invalid theme JSON in %1: %2").arg(path, parseError.errorString());
        return false;
    }

    *theme = doc.object();
    return true;
}

QString ThemeManager::stringValue(const QString &key) const
{
    return m_theme.value(key).toString();
}

QColor ThemeManager::colorValue(const QString &key) const
{
    const QColor color(stringValue(key));
    return color.isValid() ? color : QColor();
}

int ThemeManager::intValue(const QString &key) const
{
    const QJsonValue value = m_theme.value(key);
    if (value.isDouble())
        return value.toInt();
    return value.toString().toInt();
}

QString ThemeManager::qmlPath(const QString &path)
{
    if (path.startsWith(QStringLiteral(":/")))
        return QStringLiteral("qrc") + path;
    return path;
}
