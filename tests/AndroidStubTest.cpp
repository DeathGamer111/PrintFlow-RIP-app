#include "ImageLoader.h"
#include "PrintJobCMYK.h"
#include "PrintJobMultiInk.h"

#include <QtTest/QtTest>
#include <QImage>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUrl>

class AndroidStubTest : public QObject
{
    Q_OBJECT

private slots:
    void cmykStubFailsCleanly();
    void multiInkStubFailsCleanly();
    void imageLoaderStubHandlesLocalFiles();
};

void AndroidStubTest::cmykStubFailsCleanly()
{
    PrintJobCMYK job;
    QSignalSpy spy(&job, &PrintJobCMYK::prnGenerationFinished);

    job.prepareAssets();
    QVERIFY(job.getAvailableICCProfiles().size() >= 4);
    QVERIFY(!job.getDefaultOutputICCProfile().isEmpty());
    QVERIFY(!job.loadInputImage(QStringLiteral("/tmp/missing.png")));
    QVERIFY(!job.applyICCConversion(QString(), QString()));
    QVERIFY(!job.generateFinalPRN(QStringLiteral("/tmp/out.prn"), 720, 600));

    job.runPRNGeneration({}, QStringLiteral("/tmp/out.prn"));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toBool(), false);
}

void AndroidStubTest::multiInkStubFailsCleanly()
{
    PrintJobMultiInk job;
    QSignalSpy spy(&job, &PrintJobMultiInk::prnGenerationFinished);

    job.setInkMode(10);
    QCOMPARE(job.inkMode(), 10);
    job.setInkMode(9);
    QCOMPARE(job.inkMode(), 4);

    QVERIFY(job.prepareAssets());
    QVERIFY(job.getAvailableICCProfiles().size() >= 4);
    QVERIFY(!job.loadInputImage(QStringLiteral("/tmp/missing.png")));
    QVERIFY(!job.generateFinalPRN(QStringLiteral("/tmp/out.prn"), 720, 600));

    job.runDirectPrint({});
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toBool(), false);
}

void AndroidStubTest::imageLoaderStubHandlesLocalFiles()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString path = dir.filePath(QStringLiteral("tiny.png"));
    QImage image(1, 1, QImage::Format_RGB32);
    image.fill(Qt::black);
    QVERIFY(image.save(path));

    ImageLoader loader;
    QVERIFY(loader.isSupportedExtension(path));
    QVERIFY(loader.validateFile(QUrl::fromLocalFile(path).toString()));

    const QVariantMap metadata = loader.extractMetadata(path);
    QCOMPARE(metadata.value(QStringLiteral("fileName")).toString(), QStringLiteral("tiny.png"));
    QCOMPARE(metadata.value(QStringLiteral("width")).toInt(), 1);
    QCOMPARE(metadata.value(QStringLiteral("height")).toInt(), 1);
}

QTEST_GUILESS_MAIN(AndroidStubTest)
#include "AndroidStubTest.moc"
