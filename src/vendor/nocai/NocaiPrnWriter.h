#pragma once

#include <QString>

#include <cstdint>
#include <vector>

class NocaiPrnWriter
{
public:
    enum class MultiInkMode {
        FourColorYMCK = 4,
        FiveColorYMCKW = 5,
        SixColorYMCKLmLc = 6,
        SevenColorYMCKLmLcW = 7,
        EightColorYMCKLmLcLkLLk = 8,
        TenColorYMCKLmLcLkLLkWV = 10
    };

    static std::vector<std::vector<uint8_t>> packTo2Bpp(
        const std::vector<std::vector<uint8_t>>& dotMap,
        int width,
        int height);

    static bool writeStandardCmykPrn(
        const std::vector<std::vector<std::vector<uint8_t>>>& packedLines,
        const std::vector<int>& channelOrder,
        int width,
        int height,
        int xdpi,
        int ydpi,
        const QString& outputPath);

    static bool writeMultiInkPrn(
        const std::vector<std::vector<std::vector<uint8_t>>>& packedLines,
        const std::vector<int>& channelOrder,
        MultiInkMode mode,
        int width,
        int height,
        int xdpi,
        int ydpi,
        int bytesPerLine,
        const QString& outputPath);
};
