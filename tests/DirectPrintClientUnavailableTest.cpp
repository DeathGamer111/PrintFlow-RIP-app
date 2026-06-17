#include "NocaiDirectPrintClient.h"

#include <QCoreApplication>
#include <QTemporaryDir>

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    QTemporaryDir emptySdkRoot;
    if (!emptySdkRoot.isValid())
        return 1;

    NocaiDirectPrintClient client;
    client.setSdkRootPath(emptySdkRoot.path());

    IPrintOutputClient* outputClient = &client;
    if (outputClient->vendorName().isEmpty())
        return 2;

    if (outputClient->isAvailable())
        return 3;

    DirectPrintRaster raster;
    DirectPrintSettings settings;
    if (outputClient->submitPreparedJob(raster, settings))
        return 4;

    return outputClient->lastError().isEmpty() ? 5 : 0;
}
