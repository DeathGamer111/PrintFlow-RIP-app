#include "PrintJobCMYK.h"

#include <QFileInfo>
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QVariantList>
#include <QVariantMap>

class PrintJobCMYKAssetManagerTest : public QObject
{
    Q_OBJECT

private slots:
    void preparesBundledProfilesIntoRuntimePath();
};

void PrintJobCMYKAssetManagerTest::preparesBundledProfilesIntoRuntimePath()
{
    QTemporaryDir dataHome;
    QVERIFY(dataHome.isValid());

    qputenv("XDG_DATA_HOME", dataHome.path().toUtf8());

    PrintJobCMYK printJob;
    printJob.prepareAssets();

    const QString outputProfile = printJob.getDefaultOutputICCProfile();
    const QString inputProfile = printJob.getDefaultInputCMYKProfile();
    QVERIFY(!outputProfile.isEmpty());
    QVERIFY(!inputProfile.isEmpty());
    QVERIFY(QFileInfo::exists(outputProfile));
    QVERIFY(QFileInfo::exists(inputProfile));

    const QVariantList profiles = printJob.getAvailableICCProfiles();
    QVERIFY(profiles.size() >= 4);

    bool sawSrgb = false;
    bool sawCmyk = false;
    for (const QVariant& value : profiles) {
        const QVariantMap profile = value.toMap();
        const QString path = profile.value(QStringLiteral("path")).toString();
        if (path.endsWith(QStringLiteral("sRGBProfile.icm")) && QFileInfo::exists(path))
            sawSrgb = true;
        if (path.endsWith(QStringLiteral("RIP_App_Generic_CMYK.icc")) && QFileInfo::exists(path))
            sawCmyk = true;
    }

    QVERIFY(sawSrgb);
    QVERIFY(sawCmyk);

    printJob.cleanupRuntimeAssets();
    QVERIFY(!QFileInfo::exists(outputProfile));
    QVERIFY(!QFileInfo::exists(inputProfile));
}

QTEST_GUILESS_MAIN(PrintJobCMYKAssetManagerTest)
#include "PrintJobCMYKAssetManagerTest.moc"
