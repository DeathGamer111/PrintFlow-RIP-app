#ifndef THEMEMANAGER_H
#define THEMEMANAGER_H

#include <QObject>
#include <QColor>
#include <QJsonObject>

class ThemeManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString id READ id NOTIFY themeChanged)
    Q_PROPERTY(QString displayName READ displayName NOTIFY themeChanged)
    Q_PROPERTY(QString appName READ appName NOTIFY themeChanged)
    Q_PROPERTY(QColor primaryColor READ primaryColor NOTIFY themeChanged)
    Q_PROPERTY(QColor secondaryColor READ secondaryColor NOTIFY themeChanged)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor NOTIFY themeChanged)
    Q_PROPERTY(QColor surfaceColor READ surfaceColor NOTIFY themeChanged)
    Q_PROPERTY(QColor surface2Color READ surface2Color NOTIFY themeChanged)
    Q_PROPERTY(QColor textColor READ textColor NOTIFY themeChanged)
    Q_PROPERTY(QColor subtextColor READ subtextColor NOTIFY themeChanged)
    Q_PROPERTY(QColor dividerColor READ dividerColor NOTIFY themeChanged)
    Q_PROPERTY(QColor lightBackgroundColor READ lightBackgroundColor NOTIFY themeChanged)
    Q_PROPERTY(QColor lightSurfaceColor READ lightSurfaceColor NOTIFY themeChanged)
    Q_PROPERTY(QColor lightSurface2Color READ lightSurface2Color NOTIFY themeChanged)
    Q_PROPERTY(QColor lightTextColor READ lightTextColor NOTIFY themeChanged)
    Q_PROPERTY(QColor lightSubtextColor READ lightSubtextColor NOTIFY themeChanged)
    Q_PROPERTY(QColor lightDividerColor READ lightDividerColor NOTIFY themeChanged)
    Q_PROPERTY(QColor accentColor READ accentColor NOTIFY themeChanged)
    Q_PROPERTY(QString logoPath READ logoPath NOTIFY themeChanged)
    Q_PROPERTY(QString splashLogoPath READ splashLogoPath NOTIFY themeChanged)
    Q_PROPERTY(int logoWidth READ logoWidth NOTIFY themeChanged)
    Q_PROPERTY(int logoHeight READ logoHeight NOTIFY themeChanged)
    Q_PROPERTY(QString aboutVendorName READ aboutVendorName NOTIFY themeChanged)
    Q_PROPERTY(QString supportUrl READ supportUrl NOTIFY themeChanged)
    Q_PROPERTY(QString copyrightText READ copyrightText NOTIFY themeChanged)
    Q_PROPERTY(QString sourcePath READ sourcePath NOTIFY themeChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY themeChanged)

public:
    explicit ThemeManager(QObject *parent = nullptr);

    bool loadSelectedTheme();
    Q_INVOKABLE bool loadFromPath(const QString &path);

    QString id() const;
    QString displayName() const;
    QString appName() const;
    QColor primaryColor() const;
    QColor secondaryColor() const;
    QColor backgroundColor() const;
    QColor surfaceColor() const;
    QColor surface2Color() const;
    QColor textColor() const;
    QColor subtextColor() const;
    QColor dividerColor() const;
    QColor lightBackgroundColor() const;
    QColor lightSurfaceColor() const;
    QColor lightSurface2Color() const;
    QColor lightTextColor() const;
    QColor lightSubtextColor() const;
    QColor lightDividerColor() const;
    QColor accentColor() const;
    QString logoPath() const;
    QString splashLogoPath() const;
    int logoWidth() const;
    int logoHeight() const;
    QString aboutVendorName() const;
    QString supportUrl() const;
    QString copyrightText() const;
    QString sourcePath() const;
    QString lastError() const;

signals:
    void themeChanged();

private:
    bool loadTheme(const QString &path, bool requireIdentity);
    bool applyThemeObject(const QJsonObject &theme, const QString &path, bool requireIdentity);
    bool loadJsonObject(const QString &path, QJsonObject *theme);
    QString stringValue(const QString &key) const;
    QColor colorValue(const QString &key) const;
    int intValue(const QString &key) const;
    static QString qmlPath(const QString &path);

    QJsonObject m_theme;
    QString m_sourcePath;
    QString m_lastError;
};

#endif // THEMEMANAGER_H
