#pragma once

#include <QString>

#include <cstdint>
#include <vector>

struct DirectPrintSettings
{
    int printerIndex = -1;
    int printDirection = 0;
    int printSpeed = 1;
    int wcSequence = 0;
    int eclosionGrade = 0;
    int headSelect = 0;
    int whiteInkPercent = 0;
    int whiteInkPassCount = 0;
    int varnishInkPercent = 0;
    int varnishInkPassCount = 0;
    int headVoltage = 512;
    int disableUv0 = 0;
    int disableUv1 = 0;
    int disableUv2 = 0;
    int disableUv3 = 0;
    int disableUv4 = 0;
    int disableUv5 = 0;
    int carReset = 1;
    int stripBlank = 1;
    int blankDistance = 0;
    int pass = 0;
    int vsdMode = 0;
};

struct DirectPrintRaster
{
    const std::vector<std::vector<std::vector<uint8_t>>>* packedLines = nullptr;
    std::vector<int> channelOrder;
    int width = 0;
    int height = 0;
    int xdpi = 0;
    int ydpi = 0;
    int bytesPerLine = 0;
};

class IPrintOutputClient
{
public:
    virtual ~IPrintOutputClient() = default;

    virtual bool isAvailable() = 0;
    virtual QString vendorName() const = 0;
    virtual QString lastError() const = 0;
    virtual bool submitPreparedJob(const DirectPrintRaster& raster,
                                   const DirectPrintSettings& settings) = 0;
};
