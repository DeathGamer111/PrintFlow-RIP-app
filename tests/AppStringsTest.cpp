#include "AppStrings.h"

#include <QtTest/QtTest>

class AppStringsTest : public QObject
{
    Q_OBJECT

private slots:
    void stringsJsonLoads();
    void knownKeysResolve();
    void missingKeysUseVisibleFallback();
};

void AppStringsTest::stringsJsonLoads()
{
    qputenv("PRINTFLOW_LANGUAGE", "en");
    AppStrings strings;
    QCOMPARE(strings.language(), QStringLiteral("en"));
    QVERIFY(strings.availableLanguages().size() >= 2);
}

void AppStringsTest::knownKeysResolve()
{
    AppStrings strings;
    QVERIFY(strings.hasKey(QStringLiteral("app.title")));
    QCOMPARE(strings.trKey(QStringLiteral("app.title")), QStringLiteral("PrintFlow"));
}

void AppStringsTest::missingKeysUseVisibleFallback()
{
    AppStrings strings;
    const QString missing = strings.trKey(QStringLiteral("missing.test.key"));
    QCOMPARE(missing, QStringLiteral("[[missing.test.key]]"));
}

QTEST_GUILESS_MAIN(AppStringsTest)
#include "AppStringsTest.moc"
