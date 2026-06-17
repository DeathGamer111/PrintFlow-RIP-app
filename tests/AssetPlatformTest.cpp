#include "AssetManager.h"
#include "PlatformCapabilities.h"

#include <QtTest/QtTest>
#include <QTemporaryDir>

class AssetPlatformTest : public QObject
{
    Q_OBJECT

private slots:
    void assetManagerUsesWritableRuntimePath();
    void platformCapabilitiesAreConsistent();
};

void AssetPlatformTest::assetManagerUsesWritableRuntimePath()
{
    QTemporaryDir dataHome;
    QVERIFY(dataHome.isValid());
    qputenv("XDG_DATA_HOME", dataHome.path().toUtf8());

    AssetManager manager;
    QVERIFY(manager.initialize(QStringLiteral("test_assets")));
    QVERIFY(manager.rootPath().startsWith(dataHome.path()));
    QVERIFY(QFileInfo::exists(manager.rootPath()));

    const QString source = dataHome.filePath(QStringLiteral("source.txt"));
    QFile file(source);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("asset");
    file.close();

    QVERIFY(manager.copyResourceIfMissing(source, QStringLiteral("copied.txt")));
    QVERIFY(manager.hasAsset(QStringLiteral("copied.txt")));
    QVERIFY(manager.assetPath(QStringLiteral("copied.txt")).endsWith(QStringLiteral("/copied.txt")));

    QVERIFY(!manager.copyResourcesIfMissing({source}, {}));
    QVERIFY(manager.cleanup());
    QVERIFY(!QFileInfo::exists(manager.rootPath()));
}

void AssetPlatformTest::platformCapabilitiesAreConsistent()
{
    PlatformCapabilities capabilities;
#if defined(Q_OS_ANDROID)
    QCOMPARE(capabilities.platformName(), QStringLiteral("android"));
    QVERIFY(!capabilities.supportsCupsPrinting());
#elif defined(Q_OS_LINUX)
    QCOMPARE(capabilities.platformName(), QStringLiteral("linux"));
    QVERIFY(capabilities.supportsCupsPrinting());
#else
    QCOMPARE(capabilities.platformName(), QStringLiteral("desktop"));
#endif
    QVERIFY(capabilities.supportsRipProcessing());
    QVERIFY(capabilities.supportsDirectPrint());
}

QTEST_GUILESS_MAIN(AssetPlatformTest)
#include "AssetPlatformTest.moc"
