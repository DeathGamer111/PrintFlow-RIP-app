#include "NocaiPrnWriter.h"

#include <QDebug>
#include <QUrl>

#include <algorithm>
#include <cstring>
#include <fstream>

namespace {
#pragma pack(push, 1)
struct InkjetImageHeader {
    char     magic[6];
    uint16_t version;
    uint32_t pixelWidth;
    uint32_t pixelHeight;
    float    xDpi;
    float    yDpi;
    uint32_t bytesPerLine;
    uint32_t previewBytes;
    uint32_t reserved1[4];
    uint8_t  dotBits;
    uint8_t  colorNum;
    uint8_t  reserved2[14];
    uint8_t  colorList[16];
};
#pragma pack(pop)

void buildColorListForMode(
    NocaiPrnWriter::MultiInkMode mode,
    const std::vector<int>& channelOrder,
    uint8_t colorList[16])
{
    std::fill(colorList, colorList + 16, 0);

    auto mapLogicalChannelToBoardByte = [&](int ch) -> uint8_t {
        switch (mode) {
        case NocaiPrnWriter::MultiInkMode::FourColorYMCK:
            switch (ch) {
            case 0: return 'C';
            case 1: return 'M';
            case 2: return 'Y';
            case 3: return 'K';
            default: return 0;
            }

        case NocaiPrnWriter::MultiInkMode::FiveColorYMCKW:
            switch (ch) {
            case 0: return 'C';
            case 1: return 'M';
            case 2: return 'Y';
            case 3: return 'K';
            case 4: return 'W';
            default: return 0;
            }

        case NocaiPrnWriter::MultiInkMode::SixColorYMCKLmLc:
            switch (ch) {
            case 0: return 'C';
            case 1: return 'M';
            case 2: return 'Y';
            case 3: return 'K';
            case 4: return 'c';
            case 5: return 'm';
            default: return 0;
            }

        case NocaiPrnWriter::MultiInkMode::SevenColorYMCKLmLcW:
            switch (ch) {
            case 0: return 'C';
            case 1: return 'M';
            case 2: return 'Y';
            case 3: return 'K';
            case 4: return 'c';
            case 5: return 'm';
            case 6: return 'W';
            default: return 0;
            }

        case NocaiPrnWriter::MultiInkMode::EightColorYMCKLmLcLkLLk:
            switch (ch) {
            case 0: return 'C';
            case 1: return 'M';
            case 2: return 'Y';
            case 3: return 'K';
            case 4: return 'c';
            case 5: return 'm';
            case 6: return 'k';
            case 7: return 0x01;
            default: return 0;
            }

        case NocaiPrnWriter::MultiInkMode::TenColorYMCKLmLcLkLLkWV:
            switch (ch) {
            case 0: return 'C';
            case 1: return 'M';
            case 2: return 'Y';
            case 3: return 'K';
            case 4: return 'c';
            case 5: return 'm';
            case 6: return 'k';
            case 7: return 0x01;
            case 8: return 'W';
            case 9: return 'V';
            default: return 0;
            }
        }

        return 0;
    };

    const size_t count = std::min<size_t>(channelOrder.size(), 16);
    for (size_t i = 0; i < count; ++i)
        colorList[i] = mapLogicalChannelToBoardByte(channelOrder[i]);
}
}

std::vector<std::vector<uint8_t>> NocaiPrnWriter::packTo2Bpp(
    const std::vector<std::vector<uint8_t>>& dotMap,
    int width,
    int height)
{
    const int bytesPerLine = (width + 3) / 4;
    std::vector<std::vector<uint8_t>> packedLines(height);

    for (int y = 0; y < height; ++y) {
        std::vector<uint8_t>& line = packedLines[y];
        line.reserve(bytesPerLine);
        uint8_t byte = 0;
        int idx = 0;

        for (int x = 0; x < width; ++x) {
            const uint8_t level = dotMap[y][x] & 0x03;
            byte |= level << ((3 - (idx % 4)) * 2);
            ++idx;

            if (idx % 4 == 0) {
                line.push_back(byte);
                byte = 0;
            }
        }

        if (idx % 4 != 0)
            line.push_back(byte);
        while (line.size() % 4 != 0)
            line.push_back(0);
    }

    return packedLines;
}

bool NocaiPrnWriter::writeStandardCmykPrn(
    const std::vector<std::vector<std::vector<uint8_t>>>& packedLines,
    const std::vector<int>& channelOrder,
    int width,
    int height,
    int xdpi,
    int ydpi,
    const QString& outputPath)
{
    if (packedLines.empty() || packedLines[0].empty()) {
        qWarning() << "NocaiPrnWriter: standard CMYK packed lines are empty.";
        return false;
    }

    const QString outPath = QUrl(outputPath).toLocalFile();
    std::ofstream out(outPath.toStdString(), std::ios::binary);
    if (!out) {
        qWarning() << "Failed to open output file for writing:" << outputPath;
        return false;
    }

    const uint32_t bytesPerLine = static_cast<uint32_t>(packedLines[0][0].size());
    const uint32_t header[12] = {
        0x00005555,
        static_cast<uint32_t>(xdpi),
        static_cast<uint32_t>(ydpi),
        bytesPerLine,
        static_cast<uint32_t>(height),
        static_cast<uint32_t>(width),
        0,
        4,
        1,
        1,
        0,
        0
    };

    out.write(reinterpret_cast<const char*>(header), sizeof(header));

    for (int row = 0; row < height; ++row) {
        for (int ch : channelOrder)
            out.write(reinterpret_cast<const char*>(packedLines[ch][row].data()), packedLines[ch][row].size());
    }

    out.close();
    qDebug() << "Final PRN file created:" << outputPath;
    return true;
}

bool NocaiPrnWriter::writeMultiInkPrn(
    const std::vector<std::vector<std::vector<uint8_t>>>& packedLines,
    const std::vector<int>& channelOrder,
    MultiInkMode mode,
    int width,
    int height,
    int xdpi,
    int ydpi,
    int bytesPerLine,
    const QString& outputPath)
{
    if (packedLines.empty() || packedLines[0].empty()) {
        qWarning() << "NocaiPrnWriter: multi-ink packed lines are empty.";
        return false;
    }

    const uint32_t colors = static_cast<uint32_t>(channelOrder.size());
    if (colors == 0 || colors > 16) {
        qWarning() << "NocaiPrnWriter: invalid channel count for header:" << colors;
        return false;
    }

    float fxDpi = static_cast<float>(xdpi);
    float fyDpi = static_cast<float>(ydpi);

    if (fxDpi > 720.0f) {
        qWarning() << "NocaiPrnWriter: xDpi" << fxDpi << "exceeds 720, clamping.";
        fxDpi = 720.0f;
    }

    const QString outPath = QUrl(outputPath).toLocalFile();
    std::ofstream out(outPath.toStdString(), std::ios::binary);
    if (!out) {
        qWarning() << "NocaiPrnWriter: failed to open output file:" << outputPath;
        return false;
    }

    InkjetImageHeader hdr{};
    std::memset(&hdr, 0, sizeof(hdr));

    const char magicStr[6] = {'i', 'n', 'k', 'j', 'e', 't'};
    std::memcpy(hdr.magic, magicStr, sizeof(hdr.magic));

    hdr.version = 0x0001;
    hdr.pixelWidth = static_cast<uint32_t>(width);
    hdr.pixelHeight = static_cast<uint32_t>(height);
    hdr.xDpi = fxDpi;
    hdr.yDpi = fyDpi;
    hdr.bytesPerLine = static_cast<uint32_t>(bytesPerLine);
    hdr.previewBytes = 0;
    hdr.dotBits = 2;
    hdr.colorNum = static_cast<uint8_t>(colors);

    std::memset(hdr.colorList, 0x00, sizeof(hdr.colorList));
    buildColorListForMode(mode, channelOrder, hdr.colorList);

    out.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));

    for (int row = 0; row < height; ++row) {
        for (int ch : channelOrder) {
            const std::vector<uint8_t>& src = packedLines[ch][row];
            out.write(reinterpret_cast<const char*>(src.data()), src.size());
        }
    }

    out.close();
    qDebug() << "NocaiPrnWriter: PRN created at" << outputPath
             << "width" << width
             << "height" << height
             << "colors" << colors
             << "bytesPerLine" << bytesPerLine;

    return true;
}
