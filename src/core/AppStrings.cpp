#include "AppStrings.h"

#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QtGlobal>
#include <QVariantMap>

namespace {
QSettings appStringSettings()
{
    return QSettings(QSettings::IniFormat,
                     QSettings::UserScope,
                     QStringLiteral("PrintFlow"),
                     QStringLiteral("PrintFlow"));
}
}

AppStrings::AppStrings(QObject* parent)
    : QObject(parent)
{
    QSettings settings = appStringSettings();
    m_language = settings.value(QStringLiteral("ui/language"), QStringLiteral("en")).toString();
    if (qEnvironmentVariableIsSet("PRINTFLOW_LANGUAGE"))
        m_language = qEnvironmentVariable("PRINTFLOW_LANGUAGE");
    if (m_language.trimmed().isEmpty())
        m_language = QStringLiteral("en");

    loadLanguage(m_language);
}

QString AppStrings::language() const
{
    return m_language;
}

void AppStrings::setLanguage(const QString& language)
{
    const QString clean = language.trimmed().isEmpty() ? QStringLiteral("en") : language.trimmed();
    if (clean == m_language)
        return;

    if (!loadLanguage(clean)) {
        qWarning() << "AppStrings: failed to load language" << clean;
        return;
    }

    m_language = clean;
    QSettings settings = appStringSettings();
    settings.setValue(QStringLiteral("ui/language"), m_language);
    emit languageChanged();
}

QVariantList AppStrings::availableLanguages() const
{
    return QVariantList {
        QVariantMap {
            { QStringLiteral("code"), QStringLiteral("en") },
            { QStringLiteral("name"), QStringLiteral("English") },
            { QStringLiteral("nativeName"), QStringLiteral("English") }
        },
        QVariantMap {
            { QStringLiteral("code"), QStringLiteral("zh-Hans") },
            { QStringLiteral("name"), QStringLiteral("Chinese (Simplified)") },
            { QStringLiteral("nativeName"), QStringLiteral("简体中文") }
        }
    };
}

QString AppStrings::trKey(const QString& key) const
{
    const auto it = m_strings.constFind(key);
    if (it != m_strings.constEnd())
        return it.value();

    qWarning() << "AppStrings: missing string key:" << key;
    return fallbackForMissingKey(key);
}

bool AppStrings::hasKey(const QString& key) const
{
    return m_strings.contains(key);
}

bool AppStrings::loadLanguage(const QString& language)
{
    QHash<QString, QString> previous = m_strings;
    m_strings.clear();

    const QString basePath = QStringLiteral(":/i18n/strings.json");
    if (!loadFromResource(basePath)) {
        m_strings = previous;
        return false;
    }

    if (language != QStringLiteral("en")) {
        const QString localizedPath = QStringLiteral(":/i18n/strings.%1.json").arg(language);
        if (!loadFromResource(localizedPath)) {
            qWarning() << "AppStrings: localized strings not found, using base strings for" << language;
        }
    }

    return true;
}

bool AppStrings::loadFromResource(const QString& resourcePath)
{
    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        qWarning() << "AppStrings: strings resource is not a JSON object:" << resourcePath;
        return false;
    }

    const QJsonObject obj = doc.object();
    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
        if (!it.value().isString()) {
            qWarning() << "AppStrings: non-string value for key" << it.key() << "in" << resourcePath;
            continue;
        }
        m_strings.insert(it.key(), it.value().toString());
    }

    qDebug() << "AppStrings: loaded" << m_strings.size() << "strings from" << resourcePath;
    return true;
}

QString AppStrings::fallbackForMissingKey(const QString& key) const
{
    return QStringLiteral("[[%1]]").arg(key);
}
