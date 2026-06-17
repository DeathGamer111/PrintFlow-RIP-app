#pragma once

#include <QObject>
#include <QHash>
#include <QString>

class AppStrings : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)

public:
    explicit AppStrings(QObject* parent = nullptr);

    QString language() const;
    void setLanguage(const QString& language);

    Q_INVOKABLE QString trKey(const QString& key) const;
    Q_INVOKABLE bool hasKey(const QString& key) const;

signals:
    void languageChanged();

private:
    bool loadLanguage(const QString& language);
    bool loadFromResource(const QString& resourcePath);
    QString fallbackForMissingKey(const QString& key) const;

    QString m_language = QStringLiteral("en");
    QHash<QString, QString> m_strings;
};
