#include "PrintJobOutput.h"

#include <QDebug>

PrintJobOutput::PrintJobOutput(QObject* parent)
    : QObject(parent)
{
}

QStringList PrintJobOutput::detectedPrinters() const
{
    return {};
}

void PrintJobOutput::refreshDetectedPrinters()
{
    emit detectedPrintersChanged();
}

bool PrintJobOutput::loadPrinter(const QString& printerName)
{
    Q_UNUSED(printerName)
    qWarning() << "CUPS printer loading is not available on Android.";
    return false;
}

bool PrintJobOutput::loadPPDFile(const QString& ppdPath)
{
    Q_UNUSED(ppdPath)
    qWarning() << "PPD loading is not available on Android.";
    return false;
}

bool PrintJobOutput::registerPrinterFromPPD(const QString& printerName, const QString& ppdPath)
{
    Q_UNUSED(printerName)
    Q_UNUSED(ppdPath)
    qWarning() << "CUPS printer registration is not available on Android.";
    return false;
}

QStringList PrintJobOutput::supportedResolutions() const
{
    return {};
}

QStringList PrintJobOutput::supportedMediaSizes() const
{
    return {};
}

QStringList PrintJobOutput::supportedDuplexModes() const
{
    return {};
}

QStringList PrintJobOutput::supportedColorModes() const
{
    return {};
}

bool PrintJobOutput::isOptionSupported(const QString& option) const
{
    Q_UNUSED(option)
    return false;
}

bool PrintJobOutput::isOptionValueSupported(const QString& option, const QString& value) const
{
    Q_UNUSED(option)
    Q_UNUSED(value)
    return false;
}

QString PrintJobOutput::getDefaultOptionValue(const QString& option) const
{
    Q_UNUSED(option)
    return {};
}

QStringList PrintJobOutput::getSupportedValues(const QString& option) const
{
    Q_UNUSED(option)
    return {};
}

bool PrintJobOutput::generatePRN(const PrintJob& job, const QString& inputFile, const QString& outputPath)
{
    Q_UNUSED(job)
    Q_UNUSED(inputFile)
    Q_UNUSED(outputPath)
    qWarning() << "CUPS PRN generation is not available on Android.";
    return false;
}

bool PrintJobOutput::generatePRN(const QVariantMap& jobMap, const QString& inputFile, const QString& outputPath)
{
    Q_UNUSED(jobMap)
    Q_UNUSED(inputFile)
    Q_UNUSED(outputPath)
    qWarning() << "CUPS PRN generation is not available on Android.";
    return false;
}

bool PrintJobOutput::generatePRNviaFilter(const PrintJob& job, const QString ppdPath, const QString& inputFile, const QString& outputPath)
{
    Q_UNUSED(job)
    Q_UNUSED(ppdPath)
    Q_UNUSED(inputFile)
    Q_UNUSED(outputPath)
    qWarning() << "cupsfilter is not available on Android.";
    return false;
}

bool PrintJobOutput::generatePRNviaFilter(const QVariantMap& jobMap, const QString& inputFile, const QString& outputPath)
{
    Q_UNUSED(jobMap)
    Q_UNUSED(inputFile)
    Q_UNUSED(outputPath)
    qWarning() << "cupsfilter is not available on Android.";
    return false;
}
