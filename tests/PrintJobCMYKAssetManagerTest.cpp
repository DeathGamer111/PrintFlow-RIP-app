#include "PrintJobCMYK.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QVariantList>
#include <QVariantMap>

int main(int argc, char** argv)
{
    QTemporaryDir dataHome;
    if (!dataHome.isValid())
        return 1;

    qputenv("XDG_DATA_HOME", dataHome.path().toUtf8());

    QCoreApplication app(argc, argv);

    PrintJobCMYK printJob;
    printJob.prepareAssets();

    const QString outputProfile = printJob.getDefaultOutputICCProfile();
    const QString inputProfile = printJob.getDefaultInputCMYKProfile();
    if (outputProfile.isEmpty() || inputProfile.isEmpty())
        return 2;
    if (!QFileInfo::exists(outputProfile) || !QFileInfo::exists(inputProfile))
        return 3;

    const QVariantList profiles = printJob.getAvailableICCProfiles();
    if (profiles.size() < 4)
        return 4;

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

    if (!sawSrgb || !sawCmyk)
        return 5;

    printJob.cleanupRuntimeAssets();
    if (QFileInfo::exists(outputProfile) || QFileInfo::exists(inputProfile))
        return 6;

    return 0;
}
