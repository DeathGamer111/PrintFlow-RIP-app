#include "NocaiPrnWriter.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <QUrl>

#include <cstring>

namespace {
uint32_t readU32(const QByteArray& data, int offset)
{
    uint32_t value = 0;
    std::memcpy(&value, data.constData() + offset, sizeof(value));
    return value;
}

bool readFile(const QString& path, QByteArray& out)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    out = file.readAll();
    return true;
}
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    const std::vector<std::vector<uint8_t>> dotMap = {
        {0, 1, 2, 3, 1}
    };
    const auto packed = NocaiPrnWriter::packTo2Bpp(dotMap, 5, 1);
    if (packed.size() != 1 || packed[0].size() != 4)
        return 1;
    if (packed[0][0] != 0x1b || packed[0][1] != 0x40 || packed[0][2] != 0x00 || packed[0][3] != 0x00)
        return 2;

    QTemporaryDir tempDir;
    if (!tempDir.isValid())
        return 3;

    std::vector<std::vector<std::vector<uint8_t>>> channels(4);
    for (int ch = 0; ch < 4; ++ch)
        channels[ch] = {std::vector<uint8_t>(4, static_cast<uint8_t>(ch + 1))};
    const std::vector<int> channelOrder = {2, 1, 0, 3};

    const QString standardPath = tempDir.filePath(QStringLiteral("standard.prn"));
    if (!NocaiPrnWriter::writeStandardCmykPrn(
            channels,
            channelOrder,
            4,
            1,
            720,
            720,
            QUrl::fromLocalFile(standardPath).toString()))
        return 4;

    QByteArray standardData;
    if (!readFile(standardPath, standardData))
        return 5;
    if (standardData.size() != 64)
        return 6;
    if (readU32(standardData, 0) != 0x00005555u || readU32(standardData, 4) != 720u || readU32(standardData, 8) != 720u)
        return 7;
    if (static_cast<unsigned char>(standardData[48]) != 3 || static_cast<unsigned char>(standardData[52]) != 2 ||
        static_cast<unsigned char>(standardData[56]) != 1 || static_cast<unsigned char>(standardData[60]) != 4)
        return 8;

    const QString multiInkPath = tempDir.filePath(QStringLiteral("multiink.prn"));
    if (!NocaiPrnWriter::writeMultiInkPrn(
            channels,
            channelOrder,
            NocaiPrnWriter::MultiInkMode::FourColorYMCK,
            4,
            1,
            720,
            600,
            4,
            QUrl::fromLocalFile(multiInkPath).toString()))
        return 9;

    QByteArray multiInkData;
    if (!readFile(multiInkPath, multiInkData))
        return 10;
    if (multiInkData.size() != 96)
        return 11;
    if (QByteArray(multiInkData.constData(), 6) != QByteArray("inkjet", 6))
        return 12;
    if (static_cast<unsigned char>(multiInkData[48]) != 2 || static_cast<unsigned char>(multiInkData[49]) != 4)
        return 13;
    if (static_cast<unsigned char>(multiInkData[64]) != 'Y' ||
        static_cast<unsigned char>(multiInkData[65]) != 'M' ||
        static_cast<unsigned char>(multiInkData[66]) != 'C' ||
        static_cast<unsigned char>(multiInkData[67]) != 'K')
        return 14;

    return 0;
}
