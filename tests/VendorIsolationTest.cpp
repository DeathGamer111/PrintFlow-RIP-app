#include "NocaiDirectPrintClient.h"

#include <QtTest/QtTest>
#include <QTemporaryDir>

class VendorIsolationTest : public QObject
{
    Q_OBJECT

private slots:
    void directPrintUnavailablePathFailsCleanly();
    void missingSdkEnvironmentIsNotRequired();
};

void VendorIsolationTest::directPrintUnavailablePathFailsCleanly()
{
    QTemporaryDir emptySdkRoot;
    QVERIFY(emptySdkRoot.isValid());

    NocaiDirectPrintClient client;
    client.setSdkRootPath(emptySdkRoot.path());

    IPrintOutputClient* outputClient = &client;
    QVERIFY(!outputClient->vendorName().isEmpty());
    QVERIFY(!outputClient->isAvailable());

    DirectPrintRaster raster;
    DirectPrintSettings settings;
    QVERIFY(!outputClient->submitPreparedJob(raster, settings));
    QVERIFY(!outputClient->lastError().isEmpty());
}

void VendorIsolationTest::missingSdkEnvironmentIsNotRequired()
{
    qunsetenv("DIRECT_PRINT_SDK_ROOT");

    NocaiDirectPrintClient client;
    QVERIFY(!client.vendorName().isEmpty());
    QVERIFY(!client.isAvailable());
    QVERIFY(!client.lastError().isEmpty());
}

QTEST_GUILESS_MAIN(VendorIsolationTest)
#include "VendorIsolationTest.moc"
