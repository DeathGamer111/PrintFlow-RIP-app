#include "NocaiDirectPrintClient.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutexLocker>
#include <QUrl>
#include <QVariantMap>

#include <algorithm>
#include <cstring>

namespace {
static constexpr int kSySucceeded = 0x01;
static constexpr int kMaxPrinters = 100;
static constexpr uint32_t kPrintSignature = 0x74667966u;

struct ScopedCurrentDir
{
    explicit ScopedCurrentDir(const QString& next)
        : previous(QDir::currentPath()),
          changed(QDir::setCurrent(next))
    {
    }

    ~ScopedCurrentDir()
    {
        if (changed)
            QDir::setCurrent(previous);
    }

    QString previous;
    bool changed = false;
};

static int clampInt(int value, int lo, int hi)
{
    return std::max(lo, std::min(hi, value));
}
}

struct NocaiDirectPrintClient::PrinterInfoList
{
    int totalNum = 0;
    char infoList[kMaxPrinters][256] = {};
};

#pragma pack(push, 1)
struct NocaiDirectPrintClient::PrintJobProperty
{
    uint32_t Signature = 0;
    uint32_t XDPI = 0;
    uint32_t YDPI = 0;
    uint32_t BytesPerLine = 0;
    uint32_t Height = 0;
    uint32_t Width = 0;
    uint32_t PaperWidth = 0;
    uint16_t Colors = 0;
    uint16_t Bits = 0;
    uint32_t Pass = 0;
    uint32_t VsdMode = 0;
    uint32_t Reserved[2] = {0, 0};
};
#pragma pack(pop)

struct NocaiDirectPrintClient::JobSettings
{
    uint16_t PrintDirection = 0;
    uint16_t PrintSpeed = 1;
    uint16_t WCSequence = 0;
    uint16_t EclosionGrade = 0;
    uint16_t HeadSelect = 0;
    uint16_t WInkPercent = 0;
    uint16_t WInkPassCount = 0;
    uint16_t VInkPercent = 0;
    uint16_t VInkPassCount = 0;
    uint16_t HeadVoltage = 512;
    unsigned char DisableUVLights[6] = {0, 0, 0, 0, 0, 0};
    uint16_t CarReset = 1;
    uint16_t stripBlank = 1;
    uint16_t blankDistance = 0;
};

struct NocaiDirectPrintClient::AlignmentValues
{
    uint32_t StepValue = 0;
    unsigned char BidiValue = 0;
    int16_t HorizontalSpacing[4] = {0, 0, 0, 0};
    int16_t VerticalSpacing[4] = {0, 0, 0, 0};
    unsigned char HorizontalAlignReference = 0;
    unsigned char VerticalAlignReference = 0;
    char LeftChannelAlign_H1[8] = {};
    char LeftChannelAlign_H2[8] = {};
    char LeftChannelAlign_H3[8] = {};
    char LeftChannelAlign_H4[8] = {};
    char RightChannelAlign_H1[8] = {};
    char RightChannelAlign_H2[8] = {};
    char RightChannelAlign_H3[8] = {};
    char RightChannelAlign_H4[8] = {};
};

struct NocaiDirectPrintClient::PrinterStatus
{
    uint16_t PrintStatus = 0;
    uint16_t CleanStatus = 0;
};

struct NocaiDirectPrintClient::PrinterInfo
{
    uint16_t Mainboard_fpgaVer = 0;
    unsigned char Mainboard_fpgaExVer = 0;
    unsigned char Mainboard_fpgaSubVer = 0;
    uint16_t Carboard_fpgaVer = 0;
    unsigned char Carboard_fpgaExVer = 0;
    unsigned char Carboard_fpgaSubVer = 0;
    uint16_t Mainboard_cpuVer = 0;
    unsigned char Mainboard_cpuExVer = 0;
    unsigned char Mainboard_cpuSubVer = 0;
    uint16_t Carboard_cpuVer = 0;
    unsigned char Carboard_cpuExVer = 0;
    unsigned char Carboard_cpuSubVer = 0;
    uint32_t CarParaCRC = 0;
    uint16_t UI_CRC = 0;
    uint16_t UI_CRC2 = 0;
    unsigned char ID1 = 0;
    unsigned char ID2 = 0;
};

struct NocaiDirectPrintClient::UVParamValues
{
    int16_t RightR2LOffset = 0;
    int16_t RightL2ROffset = 0;
    int16_t LeftR2LOffset = 0;
    int16_t LeftL2ROffset = 0;
    int16_t LampL2ROffset = 0;
};

struct NocaiDirectPrintClient::NewUVParamValues
{
    int16_t UVLampLeftStartOffset = 0;
    int16_t UVLampLeftEndOffset = 0;
    int16_t UVLampLeftMinOffset = 0;
    int16_t UVLampRightStartOffset = 0;
    int16_t UVLampRightEndOffset = 0;
    int16_t UVLampRightMinOffset = 0;
    int16_t UVLampDelayDistance = 0;
};

NocaiDirectPrintClient::NocaiDirectPrintClient(QObject* parent)
    : QObject(parent)
{
}

bool NocaiDirectPrintClient::isAvailable()
{
    QMutexLocker locker(&m_mutex);
    return ensureLoaded();
}

QString NocaiDirectPrintClient::lastError() const
{
    QMutexLocker locker(&m_mutex);
    return m_lastError;
}

QString NocaiDirectPrintClient::sdkRootPath() const
{
    QMutexLocker locker(&m_mutex);
    return m_sdkRootPath;
}

void NocaiDirectPrintClient::setSdkRootPath(const QString& path)
{
    QMutexLocker locker(&m_mutex);
    const QString clean = path.trimmed();
    if (m_sdkRootPath == clean)
        return;

    m_sdkRootPath = clean;
    m_resolvedSdkRoot.clear();
    m_library.unload();
    m_searchPrinter = nullptr;
    m_choosePrinter = nullptr;
    m_initPrinter = nullptr;
    m_startPrint = nullptr;
    m_printALine = nullptr;
    m_abortPrint = nullptr;
    m_pausePrint = nullptr;
    m_continuePrint = nullptr;
    m_endPrint = nullptr;
    m_closePrint = nullptr;
    m_setJobSettings = nullptr;
    m_getJobSettings = nullptr;
    m_connectPrinter = nullptr;
    m_wipePrintHead = nullptr;
    m_startCleanOperation = nullptr;
    m_startPump = nullptr;
    m_stopPumpOperation = nullptr;
    m_spitPrintHead = nullptr;
    m_stopSpitOperation = nullptr;
    m_capPrintHead = nullptr;
    m_moveAxis = nullptr;
    m_stopAxis = nullptr;
    m_saveAxisPos = nullptr;
    m_setPrintHeight = nullptr;
    m_getPrintHeight = nullptr;
    m_setAlignmentValues = nullptr;
    m_getAlignmentValues = nullptr;
    m_exportConfigFile = nullptr;
    m_importConfigFile = nullptr;
    m_printAlignmentPattern = nullptr;
    m_getPrinterStatus = nullptr;
    m_getPrinterInfo = nullptr;
    m_setPrintXYValue = nullptr;
    m_getPrintXYValue = nullptr;
    m_setUVParamValues = nullptr;
    m_getUVParamValues = nullptr;
    m_getSupportNewUVParamFunction = nullptr;
    m_setNewUVParamFunction = nullptr;
    m_setNewUVParamValues = nullptr;
    m_getNewUVParamValues = nullptr;
    emit statusChanged();
}

QVariantList NocaiDirectPrintClient::printers() const
{
    QMutexLocker locker(&m_mutex);
    return m_printers;
}

QStringList NocaiDirectPrintClient::maintenanceSupportedPrinters() const
{
    return {
        QStringLiteral("X-36NC (Photo Printer)")
    };
}

bool NocaiDirectPrintClient::supportsMaintenance(const QString& printerName) const
{
    return maintenanceSupportedPrinters().contains(printerName.trimmed(), Qt::CaseInsensitive);
}

QVariantList NocaiDirectPrintClient::searchPrinters()
{
    refreshPrinters();
    return printers();
}

bool NocaiDirectPrintClient::refreshPrinters()
{
    QMutexLocker locker(&m_mutex);
    if (!ensureLoaded())
        return false;

    bool ok = false;
    PrinterInfoList list;
    withSdkWorkingDirectory([&]() {
        const int result = m_searchPrinter(&list, sizeof(PrinterInfoList));
        ok = callSucceeded(result, "SearchPrinter");
        return ok;
    });

    QVariantList found;
    if (ok) {
        const int total = clampInt(list.totalNum, 0, kMaxPrinters);
        for (int i = 0; i < total; ++i) {
            QVariantMap entry;
            entry["index"] = i;
            entry["name"] = QString::fromLocal8Bit(list.infoList[i]).trimmed();
            found.append(entry);
        }
    }

    m_printers = found;
    emit printersChanged();
    emit statusChanged();
    return ok;
}

bool NocaiDirectPrintClient::choosePrinter(int index)
{
    QMutexLocker locker(&m_mutex);
    if (!ensureLoaded())
        return false;

    bool ok = false;
    withSdkWorkingDirectory([&]() {
        ok = callSucceeded(m_choosePrinter(index), "ChoosePrinter");
        return ok;
    });
    emit statusChanged();
    return ok;
}

bool NocaiDirectPrintClient::abortPrint()
{
    QMutexLocker locker(&m_mutex);
    if (!ensureLoaded())
        return false;
    return callSucceeded(m_abortPrint(), "AbortPrint");
}

bool NocaiDirectPrintClient::pausePrint()
{
    QMutexLocker locker(&m_mutex);
    if (!ensureLoaded())
        return false;
    return callSucceeded(m_pausePrint(), "PausePrint");
}

bool NocaiDirectPrintClient::continuePrint()
{
    QMutexLocker locker(&m_mutex);
    if (!ensureLoaded())
        return false;
    return callSucceeded(m_continuePrint(), "ContinuePrint");
}

bool NocaiDirectPrintClient::connectPrinter()
{
    QMutexLocker locker(&m_mutex);
    if (!ensureLoaded() || !requireFunction(reinterpret_cast<const void*>(m_connectPrinter), "ConnectPrinter"))
        return false;
    bool ok = false;
    withSdkWorkingDirectory([&]() {
        ok = callSucceeded(m_connectPrinter(), "ConnectPrinter");
        return ok;
    });
    emit statusChanged();
    return ok;
}

bool NocaiDirectPrintClient::wipePrintHead(int printHeadMask)
{
    QMutexLocker locker(&m_mutex);
    if (!ensureLoaded() || !requireFunction(reinterpret_cast<const void*>(m_wipePrintHead), "WipePrintHead"))
        return false;
    return callSucceeded(m_wipePrintHead(printHeadMask), "WipePrintHead");
}

bool NocaiDirectPrintClient::startCleanOperation(int printHeadMask)
{
    QMutexLocker locker(&m_mutex);
    if (!ensureLoaded() || !requireFunction(reinterpret_cast<const void*>(m_startCleanOperation), "StartCleanOperation"))
        return false;
    return callSucceeded(m_startCleanOperation(printHeadMask), "StartCleanOperation");
}

bool NocaiDirectPrintClient::startPump(int printHeadMask)
{
    QMutexLocker locker(&m_mutex);
    if (!ensureLoaded() || !requireFunction(reinterpret_cast<const void*>(m_startPump), "StartPump"))
        return false;
    return callSucceeded(m_startPump(printHeadMask), "StartPump");
}

bool NocaiDirectPrintClient::stopPumpOperation()
{
    QMutexLocker locker(&m_mutex);
    if (!ensureLoaded() || !requireFunction(reinterpret_cast<const void*>(m_stopPumpOperation), "StopPumpOperation"))
        return false;
    return callSucceeded(m_stopPumpOperation(), "StopPumpOperation");
}

bool NocaiDirectPrintClient::spitPrintHead(int printHeadMask)
{
    QMutexLocker locker(&m_mutex);
    if (!ensureLoaded() || !requireFunction(reinterpret_cast<const void*>(m_spitPrintHead), "SpitPrintHead"))
        return false;
    return callSucceeded(m_spitPrintHead(printHeadMask), "SpitPrintHead");
}

bool NocaiDirectPrintClient::stopSpitOperation()
{
    QMutexLocker locker(&m_mutex);
    if (!ensureLoaded() || !requireFunction(reinterpret_cast<const void*>(m_stopSpitOperation), "StopSpitOperation"))
        return false;
    return callSucceeded(m_stopSpitOperation(), "StopSpitOperation");
}

bool NocaiDirectPrintClient::capPrintHead()
{
    QMutexLocker locker(&m_mutex);
    if (!ensureLoaded() || !requireFunction(reinterpret_cast<const void*>(m_capPrintHead), "CapPrintHead"))
        return false;
    return callSucceeded(m_capPrintHead(), "CapPrintHead");
}

bool NocaiDirectPrintClient::moveAxis(int axis, int direction)
{
    QMutexLocker locker(&m_mutex);
    if (!ensureLoaded() || !requireFunction(reinterpret_cast<const void*>(m_moveAxis), "MoveAxis"))
        return false;
    return callSucceeded(m_moveAxis(clampInt(axis, 0, 2), clampInt(direction, 0, 1)), "MoveAxis");
}

QVariantMap NocaiDirectPrintClient::stopAxis(int axis)
{
    QMutexLocker locker(&m_mutex);
    QVariantMap out;
    int stopPos = 0;
    const bool ok = ensureLoaded() &&
        requireFunction(reinterpret_cast<const void*>(m_stopAxis), "StopAxis") &&
        callSucceeded(m_stopAxis(clampInt(axis, 0, 2), &stopPos), "StopAxis");
    out["ok"] = ok;
    out["position"] = stopPos;
    return out;
}

QVariantMap NocaiDirectPrintClient::saveAxisPos(int axis)
{
    QMutexLocker locker(&m_mutex);
    QVariantMap out;
    int savePos = 0;
    const bool ok = ensureLoaded() &&
        requireFunction(reinterpret_cast<const void*>(m_saveAxisPos), "SaveAxisPos") &&
        callSucceeded(m_saveAxisPos(clampInt(axis, 0, 2), &savePos), "SaveAxisPos");
    out["ok"] = ok;
    out["position"] = savePos;
    return out;
}

bool NocaiDirectPrintClient::setPrintHeight(int heightMm)
{
    QMutexLocker locker(&m_mutex);
    if (!ensureLoaded() || !requireFunction(reinterpret_cast<const void*>(m_setPrintHeight), "SetPrintHeight"))
        return false;
    return callSucceeded(m_setPrintHeight(static_cast<uint16_t>(clampInt(heightMm, 0, 65535))), "SetPrintHeight");
}

QVariantMap NocaiDirectPrintClient::getPrintHeight()
{
    QMutexLocker locker(&m_mutex);
    QVariantMap out;
    uint16_t height = 0;
    const bool ok = ensureLoaded() &&
        requireFunction(reinterpret_cast<const void*>(m_getPrintHeight), "GetPrintHeight") &&
        callSucceeded(m_getPrintHeight(&height), "GetPrintHeight");
    out["ok"] = ok;
    out["heightMm"] = static_cast<int>(height);
    return out;
}

QVariantMap NocaiDirectPrintClient::getJobSettings()
{
    QMutexLocker locker(&m_mutex);
    JobSettings settings;
    const bool ok = ensureLoaded() &&
        requireFunction(reinterpret_cast<const void*>(m_getJobSettings), "GetJobSettings") &&
        callSucceeded(m_getJobSettings(&settings, sizeof(JobSettings)), "GetJobSettings");
    QVariantMap out = jobSettingsToMap(settings);
    out["ok"] = ok;
    return out;
}

bool NocaiDirectPrintClient::setJobSettingsFromMap(const QVariantMap& settings)
{
    QMutexLocker locker(&m_mutex);
    if (!ensureLoaded() || !requireFunction(reinterpret_cast<const void*>(m_setJobSettings), "SetJobSettings"))
        return false;
    JobSettings jobSettings = jobSettingsFromMap(settings);
    return callSucceeded(m_setJobSettings(&jobSettings, sizeof(JobSettings)), "SetJobSettings");
}

bool NocaiDirectPrintClient::exportConfigFile(const QString& path)
{
    QMutexLocker locker(&m_mutex);
    if (!ensureLoaded() || !requireFunction(reinterpret_cast<const void*>(m_exportConfigFile), "ExportConfigFile"))
        return false;
    const QString localPath = path.startsWith("file:", Qt::CaseInsensitive) ? QUrl(path).toLocalFile() : path;
    QByteArray bytes = QFile::encodeName(localPath);
    return callSucceeded(m_exportConfigFile(bytes.data()), "ExportConfigFile");
}

bool NocaiDirectPrintClient::importConfigFile(const QString& path)
{
    QMutexLocker locker(&m_mutex);
    if (!ensureLoaded() || !requireFunction(reinterpret_cast<const void*>(m_importConfigFile), "ImportConfigFile"))
        return false;
    const QString localPath = path.startsWith("file:", Qt::CaseInsensitive) ? QUrl(path).toLocalFile() : path;
    QByteArray bytes = QFile::encodeName(localPath);
    return callSucceeded(m_importConfigFile(bytes.data()), "ImportConfigFile");
}

QVariantMap NocaiDirectPrintClient::getAlignmentValues()
{
    QMutexLocker locker(&m_mutex);
    AlignmentValues values;
    const bool ok = ensureLoaded() &&
        requireFunction(reinterpret_cast<const void*>(m_getAlignmentValues), "GetAlignmentValues") &&
        callSucceeded(m_getAlignmentValues(&values, sizeof(AlignmentValues)), "GetAlignmentValues");
    QVariantMap out = alignmentValuesToMap(values);
    out["ok"] = ok;
    return out;
}

bool NocaiDirectPrintClient::setAlignmentValues(const QVariantMap& settings, int type)
{
    QMutexLocker locker(&m_mutex);
    if (!ensureLoaded() || !requireFunction(reinterpret_cast<const void*>(m_setAlignmentValues), "SetAlignmentValues"))
        return false;
    AlignmentValues values = alignmentValuesFromMap(settings);
    return callSucceeded(m_setAlignmentValues(&values, clampInt(type, 0, 5), sizeof(AlignmentValues)), "SetAlignmentValues");
}

bool NocaiDirectPrintClient::printAlignmentPattern(int type)
{
    QMutexLocker locker(&m_mutex);
    if (!ensureLoaded() || !requireFunction(reinterpret_cast<const void*>(m_printAlignmentPattern), "PrintAlignmentPattern"))
        return false;
    return callSucceeded(m_printAlignmentPattern(clampInt(type, 0, 22)), "PrintAlignmentPattern");
}

QVariantMap NocaiDirectPrintClient::getPrinterStatus()
{
    QMutexLocker locker(&m_mutex);
    PrinterStatus status;
    const bool ok = ensureLoaded() &&
        requireFunction(reinterpret_cast<const void*>(m_getPrinterStatus), "GetPrinterStatus") &&
        callSucceeded(m_getPrinterStatus(&status, sizeof(PrinterStatus)), "GetPrinterStatus");
    QVariantMap out;
    out["ok"] = ok;
    out["printStatus"] = static_cast<int>(status.PrintStatus);
    out["cleanStatus"] = static_cast<int>(status.CleanStatus);
    return out;
}

QVariantMap NocaiDirectPrintClient::getPrinterInfo()
{
    QMutexLocker locker(&m_mutex);
    PrinterInfo info;
    const bool ok = ensureLoaded() &&
        requireFunction(reinterpret_cast<const void*>(m_getPrinterInfo), "GetPrinterInfo") &&
        callSucceeded(m_getPrinterInfo(&info, sizeof(PrinterInfo)), "GetPrinterInfo");
    QVariantMap out;
    out["ok"] = ok;
    out["mainboardFpga"] = QString("%1%2%3").arg(info.Mainboard_fpgaVer).arg(QChar(info.Mainboard_fpgaExVer)).arg(QChar(info.Mainboard_fpgaSubVer));
    out["carboardFpga"] = QString("%1%2%3").arg(info.Carboard_fpgaVer).arg(QChar(info.Carboard_fpgaExVer)).arg(QChar(info.Carboard_fpgaSubVer));
    out["mainboardCpu"] = QString("%1%2%3").arg(info.Mainboard_cpuVer).arg(QChar(info.Mainboard_cpuExVer)).arg(info.Mainboard_cpuSubVer);
    out["carboardCpu"] = QString("%1%2%3").arg(info.Carboard_cpuVer).arg(QChar(info.Carboard_cpuExVer)).arg(info.Carboard_cpuSubVer);
    out["carParaCrc"] = QString::number(info.CarParaCRC, 16).toUpper();
    out["uiCrc"] = QString::number(info.UI_CRC, 16).toUpper();
    out["uiCrc2"] = QString::number(info.UI_CRC2, 16).toUpper();
    out["id1"] = QString::number(info.ID1, 16).toUpper();
    out["id2"] = QString::number(info.ID2, 16).toUpper();
    return out;
}

bool NocaiDirectPrintClient::setPrintXYValue(int xMm, int yMm)
{
    QMutexLocker locker(&m_mutex);
    if (!ensureLoaded() || !requireFunction(reinterpret_cast<const void*>(m_setPrintXYValue), "SetPrintXYValue"))
        return false;
    return callSucceeded(m_setPrintXYValue(static_cast<uint32_t>(std::max(0, xMm)),
                                           static_cast<uint32_t>(std::max(0, yMm))), "SetPrintXYValue");
}

QVariantMap NocaiDirectPrintClient::getPrintXYValue()
{
    QMutexLocker locker(&m_mutex);
    QVariantMap out;
    uint32_t x = 0, y = 0;
    const bool ok = ensureLoaded() &&
        requireFunction(reinterpret_cast<const void*>(m_getPrintXYValue), "GetPrintXYValue") &&
        callSucceeded(m_getPrintXYValue(&x, &y), "GetPrintXYValue");
    out["ok"] = ok;
    out["xMm"] = static_cast<int>(x);
    out["yMm"] = static_cast<int>(y);
    return out;
}

QVariantMap NocaiDirectPrintClient::getUVParamValues()
{
    QMutexLocker locker(&m_mutex);
    UVParamValues values;
    const bool ok = ensureLoaded() &&
        requireFunction(reinterpret_cast<const void*>(m_getUVParamValues), "GetUVParamValues") &&
        callSucceeded(m_getUVParamValues(&values, sizeof(UVParamValues)), "GetUVParamValues");
    QVariantMap out = uvParamValuesToMap(values);
    out["ok"] = ok;
    return out;
}

bool NocaiDirectPrintClient::setUVParamValues(const QVariantMap& settings, int type)
{
    QMutexLocker locker(&m_mutex);
    if (!ensureLoaded() || !requireFunction(reinterpret_cast<const void*>(m_setUVParamValues), "SetUVParamValues"))
        return false;
    UVParamValues values = uvParamValuesFromMap(settings);
    return callSucceeded(m_setUVParamValues(&values, clampInt(type, 0, 4), sizeof(UVParamValues)), "SetUVParamValues");
}

int NocaiDirectPrintClient::getSupportNewUVParamFunction()
{
    QMutexLocker locker(&m_mutex);
    if (!ensureLoaded() || !requireFunction(reinterpret_cast<const void*>(m_getSupportNewUVParamFunction), "GetSupportNewUVParamFunction"))
        return 0;
    return m_getSupportNewUVParamFunction();
}

bool NocaiDirectPrintClient::setNewUVParamFunction(int type)
{
    QMutexLocker locker(&m_mutex);
    if (!ensureLoaded() || !requireFunction(reinterpret_cast<const void*>(m_setNewUVParamFunction), "SetNewUVParamFunction"))
        return false;
    return callSucceeded(m_setNewUVParamFunction(clampInt(type, 0, 8)), "SetNewUVParamFunction");
}

QVariantMap NocaiDirectPrintClient::getNewUVParamValues()
{
    QMutexLocker locker(&m_mutex);
    NewUVParamValues values;
    const bool ok = ensureLoaded() &&
        requireFunction(reinterpret_cast<const void*>(m_getNewUVParamValues), "GetNewUVParamValues") &&
        callSucceeded(m_getNewUVParamValues(&values, sizeof(NewUVParamValues)), "GetNewUVParamValues");
    QVariantMap out = newUvParamValuesToMap(values);
    out["ok"] = ok;
    return out;
}

bool NocaiDirectPrintClient::setNewUVParamValues(const QVariantMap& settings, int type)
{
    QMutexLocker locker(&m_mutex);
    if (!ensureLoaded() || !requireFunction(reinterpret_cast<const void*>(m_setNewUVParamValues), "SetNewUVParamValues"))
        return false;
    NewUVParamValues values = newUvParamValuesFromMap(settings);
    return callSucceeded(m_setNewUVParamValues(&values, clampInt(type, 0, 6), sizeof(NewUVParamValues)), "SetNewUVParamValues");
}

QString NocaiDirectPrintClient::statusText()
{
    return isAvailable()
        ? QStringLiteral("Direct print SDK ready: %1").arg(m_resolvedSdkRoot)
        : m_lastError;
}

bool NocaiDirectPrintClient::printPackedJob(const NocaiDirectPrintRaster& raster,
                                            const NocaiDirectPrintSettings& settings)
{
    QMutexLocker locker(&m_mutex);
    if (!ensureLoaded())
        return false;

    if (!raster.packedLines || raster.packedLines->empty() || raster.channelOrder.empty()) {
        setError("Direct print raster is empty.");
        return false;
    }

    const int bytesPerLine = raster.bytesPerLine > 0
        ? raster.bytesPerLine
        : static_cast<int>((*raster.packedLines)[0][0].size());

    PrintJobProperty prop;
    prop.Signature = kPrintSignature;
    prop.XDPI = static_cast<uint32_t>(std::max(1, raster.xdpi));
    prop.YDPI = static_cast<uint32_t>(std::max(1, raster.ydpi));
    prop.BytesPerLine = static_cast<uint32_t>(std::max(0, bytesPerLine));
    prop.Height = static_cast<uint32_t>(std::max(0, raster.height));
    prop.Width = static_cast<uint32_t>(std::max(0, raster.width));
    prop.PaperWidth = prop.Width;
    prop.Colors = static_cast<uint16_t>(raster.channelOrder.size());
    prop.Bits = 1;
    prop.Pass = static_cast<uint32_t>(std::max(0, settings.pass));
    prop.VsdMode = static_cast<uint32_t>(std::max(0, settings.vsdMode));

    JobSettings jobSettings = makeJobSettings(settings);
    bool ok = false;

    withSdkWorkingDirectory([&]() {
        if (settings.printerIndex >= 0 && !callSucceeded(m_choosePrinter(settings.printerIndex), "ChoosePrinter"))
            return false;

        if (!callSucceeded(m_setJobSettings(&jobSettings, sizeof(JobSettings)), "SetJobSettings"))
            return false;

        if (!callSucceeded(m_initPrinter(), "InitPrinter"))
            return false;

        if (!callSucceeded(m_startPrint(&prop), "StartPrint")) {
            m_closePrint();
            return false;
        }

        for (int row = 0; row < raster.height; ++row) {
            std::vector<uint8_t> rowBytes;
            rowBytes.reserve(static_cast<size_t>(bytesPerLine) * raster.channelOrder.size());

            for (int ch : raster.channelOrder) {
                if (ch < 0 || ch >= static_cast<int>(raster.packedLines->size()) ||
                    row < 0 || row >= static_cast<int>((*raster.packedLines)[ch].size())) {
                    setError("Direct print raster channel/row index is invalid.");
                    m_abortPrint();
                    m_closePrint();
                    return false;
                }

                const std::vector<uint8_t>& line = (*raster.packedLines)[ch][row];
                rowBytes.insert(rowBytes.end(), line.begin(), line.end());
            }

            const uint32_t expectedSize = prop.Colors * prop.BytesPerLine;
            if (rowBytes.size() != expectedSize) {
                setError(QString("Direct print row size mismatch: got %1, expected %2.")
                             .arg(rowBytes.size())
                             .arg(expectedSize));
                m_abortPrint();
                m_closePrint();
                return false;
            }

            if (!callSucceeded(m_printALine(reinterpret_cast<char*>(rowBytes.data()), expectedSize), "PrintALine")) {
                m_abortPrint();
                m_closePrint();
                return false;
            }
        }

        const bool ended = callSucceeded(m_endPrint(), "EndPrint");
        const bool closed = callSucceeded(m_closePrint(), "ClosePrint");
        ok = ended && closed;
        return ok;
    });

    emit statusChanged();
    return ok;
}

bool NocaiDirectPrintClient::ensureLoaded()
{
    if (m_library.isLoaded())
        return true;

    const QString root = resolveSdkRoot();
    if (root.isEmpty()) {
        setError("Direct print SDK folder was not found.");
        return false;
    }

    const QString libraryPath = QDir(root).filePath("libSYPrintAPIforPROII.so");
    if (!QFileInfo::exists(libraryPath)) {
        setError(QString("Direct print SDK library is missing: %1").arg(libraryPath));
        return false;
    }

    m_library.setFileName(libraryPath);
    if (!m_library.load()) {
        setError(QString("Failed to load direct print SDK: %1").arg(m_library.errorString()));
        return false;
    }

    m_resolvedSdkRoot = root;
    return resolveSymbols();
}

bool NocaiDirectPrintClient::resolveSymbols()
{
    auto resolve = [&](auto& fn, const char* name) -> bool {
        fn = reinterpret_cast<std::remove_reference_t<decltype(fn)>>(m_library.resolve(name));
        if (!fn) {
            setError(QString("Failed to resolve direct print SDK function: %1").arg(name));
            return false;
        }
        return true;
    };

    auto resolveOptional = [&](auto& fn, const char* name) {
        fn = reinterpret_cast<std::remove_reference_t<decltype(fn)>>(m_library.resolve(name));
        if (!fn)
            qWarning() << "NocaiDirectPrintClient: optional SDK function unavailable:" << name;
    };

    resolveOptional(m_getJobSettings, "GetJobSettings");
    resolveOptional(m_connectPrinter, "ConnectPrinter");
    resolveOptional(m_wipePrintHead, "WipePrintHead");
    resolveOptional(m_startCleanOperation, "StartCleanOperation");
    resolveOptional(m_startPump, "StartPump");
    resolveOptional(m_stopPumpOperation, "StopPumpOperation");
    resolveOptional(m_spitPrintHead, "SpitPrintHead");
    resolveOptional(m_stopSpitOperation, "StopSpitOperation");
    resolveOptional(m_capPrintHead, "CapPrintHead");
    resolveOptional(m_moveAxis, "MoveAxis");
    resolveOptional(m_stopAxis, "StopAxis");
    resolveOptional(m_saveAxisPos, "SaveAxisPos");
    resolveOptional(m_setPrintHeight, "SetPrintHeight");
    resolveOptional(m_getPrintHeight, "GetPrintHeight");
    resolveOptional(m_setAlignmentValues, "SetAlignmentValues");
    resolveOptional(m_getAlignmentValues, "GetAlignmentValues");
    resolveOptional(m_exportConfigFile, "ExportConfigFile");
    resolveOptional(m_importConfigFile, "ImportConfigFile");
    resolveOptional(m_printAlignmentPattern, "PrintAlignmentPattern");
    resolveOptional(m_getPrinterStatus, "GetPrinterStatus");
    resolveOptional(m_getPrinterInfo, "GetPrinterInfo");
    resolveOptional(m_setPrintXYValue, "SetPrintXYValue");
    resolveOptional(m_getPrintXYValue, "GetPrintXYValue");
    resolveOptional(m_setUVParamValues, "SetUVParamValues");
    resolveOptional(m_getUVParamValues, "GetUVParamValues");
    resolveOptional(m_getSupportNewUVParamFunction, "GetSupportNewUVParamFunction");
    resolveOptional(m_setNewUVParamFunction, "SetNewUVParamFunction");
    resolveOptional(m_setNewUVParamValues, "SetNewUVParamValues");
    resolveOptional(m_getNewUVParamValues, "GetNewUVParamValues");

    return resolve(m_searchPrinter, "SearchPrinter") &&
           resolve(m_choosePrinter, "ChoosePrinter") &&
           resolve(m_continuePrint, "ContinuePrint") &&
           resolve(m_initPrinter, "InitPrinter") &&
           resolve(m_startPrint, "StartPrint") &&
           resolve(m_printALine, "PrintALine") &&
           resolve(m_abortPrint, "AbortPrint") &&
           resolve(m_pausePrint, "PausePrint") &&
           resolve(m_endPrint, "EndPrint") &&
           resolve(m_closePrint, "ClosePrint") &&
           resolve(m_setJobSettings, "SetJobSettings");
}

QString NocaiDirectPrintClient::resolveSdkRoot() const
{
    for (const QString& candidate : sdkRootCandidates()) {
        if (candidate.trimmed().isEmpty())
            continue;

        const QDir dir(candidate);
        if (dir.exists("libSYPrintAPIforPROII.so"))
            return dir.absolutePath();
    }

    return QString();
}

QStringList NocaiDirectPrintClient::sdkRootCandidates() const
{
    QStringList roots;
    roots << m_sdkRootPath;
    roots << QCoreApplication::applicationDirPath();
#if defined(Q_OS_ANDROID)
    roots << QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("lib");
#endif
    roots << QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("DemoForARM64Linux-260612/Demo260612");
    roots << QDir::current().absoluteFilePath("DemoForARM64Linux-260612/Demo260612");
    return roots;
}

void NocaiDirectPrintClient::setError(const QString& message)
{
    if (m_lastError == message)
        return;

    m_lastError = message;
    qWarning() << "NocaiDirectPrintClient:" << message;
}

bool NocaiDirectPrintClient::callSucceeded(int result, const QString& functionName)
{
    if (result == kSySucceeded) {
        m_lastError.clear();
        return true;
    }

    setError(QString("%1 failed with SDK result 0x%2.")
                 .arg(functionName)
                 .arg(result, 0, 16));
    return false;
}

bool NocaiDirectPrintClient::requireFunction(const void* fn, const QString& functionName)
{
    if (fn)
        return true;

    setError(QString("Direct print SDK function is unavailable: %1").arg(functionName));
    return false;
}

QVariantMap NocaiDirectPrintClient::jobSettingsToMap(const JobSettings& settings) const
{
    QVariantMap out;
    out["printDirection"] = static_cast<int>(settings.PrintDirection);
    out["printSpeed"] = static_cast<int>(settings.PrintSpeed);
    out["wcSequence"] = static_cast<int>(settings.WCSequence);
    out["eclosionGrade"] = static_cast<int>(settings.EclosionGrade);
    out["headSelect"] = static_cast<int>(settings.HeadSelect);
    out["whiteInkPercent"] = static_cast<int>(settings.WInkPercent);
    out["whiteInkPassCount"] = static_cast<int>(settings.WInkPassCount);
    out["varnishInkPercent"] = static_cast<int>(settings.VInkPercent);
    out["varnishInkPassCount"] = static_cast<int>(settings.VInkPassCount);
    out["headVoltage"] = static_cast<int>(settings.HeadVoltage);
    for (int i = 0; i < 6; ++i)
        out[QString("disableUv%1").arg(i)] = static_cast<int>(settings.DisableUVLights[i]);
    out["carReset"] = static_cast<int>(settings.CarReset);
    out["stripBlank"] = static_cast<int>(settings.stripBlank);
    out["blankDistance"] = static_cast<int>(settings.blankDistance);
    return out;
}

NocaiDirectPrintClient::JobSettings
NocaiDirectPrintClient::jobSettingsFromMap(const QVariantMap& settings) const
{
    NocaiDirectPrintSettings normalized;
    auto value = [&](const char* key, int def) {
        return settings.value(QString::fromUtf8(key), def).toInt();
    };

    normalized.printDirection = value("printDirection", 0);
    normalized.printSpeed = value("printSpeed", 1);
    normalized.wcSequence = value("wcSequence", 0);
    normalized.eclosionGrade = value("eclosionGrade", 0);
    normalized.headSelect = value("headSelect", 0);
    normalized.whiteInkPercent = value("whiteInkPercent", 0);
    normalized.whiteInkPassCount = value("whiteInkPassCount", 0);
    normalized.varnishInkPercent = value("varnishInkPercent", 0);
    normalized.varnishInkPassCount = value("varnishInkPassCount", 0);
    normalized.headVoltage = value("headVoltage", 512);
    normalized.disableUv0 = value("disableUv0", 0);
    normalized.disableUv1 = value("disableUv1", 0);
    normalized.disableUv2 = value("disableUv2", 0);
    normalized.disableUv3 = value("disableUv3", 0);
    normalized.disableUv4 = value("disableUv4", 0);
    normalized.disableUv5 = value("disableUv5", 0);
    normalized.carReset = value("carReset", 1);
    normalized.stripBlank = value("stripBlank", 1);
    normalized.blankDistance = value("blankDistance", 0);
    return makeJobSettings(normalized);
}

static QVariantList byteArrayToVariantList(const char* values, int size)
{
    QVariantList out;
    for (int i = 0; i < size; ++i)
        out.append(static_cast<int>(values[i]));
    return out;
}

static void variantListToByteArray(const QVariant& value, char* out, int size)
{
    const QVariantList list = value.toList();
    for (int i = 0; i < size; ++i)
        out[i] = static_cast<char>(clampInt(i < list.size() ? list[i].toInt() : 0, -128, 127));
}

QVariantMap NocaiDirectPrintClient::alignmentValuesToMap(const AlignmentValues& values) const
{
    QVariantMap out;
    out["stepValue"] = static_cast<int>(values.StepValue);
    out["bidiValue"] = static_cast<int>(values.BidiValue);
    for (int i = 0; i < 4; ++i) {
        out[QString("horizontalSpacing%1").arg(i)] = static_cast<int>(values.HorizontalSpacing[i]);
        out[QString("verticalSpacing%1").arg(i)] = static_cast<int>(values.VerticalSpacing[i]);
    }
    out["horizontalAlignReference"] = static_cast<int>(values.HorizontalAlignReference);
    out["verticalAlignReference"] = static_cast<int>(values.VerticalAlignReference);
    out["leftChannelH1"] = byteArrayToVariantList(values.LeftChannelAlign_H1, 8);
    out["leftChannelH2"] = byteArrayToVariantList(values.LeftChannelAlign_H2, 8);
    out["leftChannelH3"] = byteArrayToVariantList(values.LeftChannelAlign_H3, 8);
    out["leftChannelH4"] = byteArrayToVariantList(values.LeftChannelAlign_H4, 8);
    out["rightChannelH1"] = byteArrayToVariantList(values.RightChannelAlign_H1, 8);
    out["rightChannelH2"] = byteArrayToVariantList(values.RightChannelAlign_H2, 8);
    out["rightChannelH3"] = byteArrayToVariantList(values.RightChannelAlign_H3, 8);
    out["rightChannelH4"] = byteArrayToVariantList(values.RightChannelAlign_H4, 8);
    return out;
}

NocaiDirectPrintClient::AlignmentValues
NocaiDirectPrintClient::alignmentValuesFromMap(const QVariantMap& settings) const
{
    AlignmentValues out;
    out.StepValue = static_cast<uint32_t>(std::max(0, settings.value("stepValue", 0).toInt()));
    out.BidiValue = static_cast<unsigned char>(clampInt(settings.value("bidiValue", 0).toInt(), 0, 255));
    for (int i = 0; i < 4; ++i) {
        out.HorizontalSpacing[i] = static_cast<int16_t>(clampInt(settings.value(QString("horizontalSpacing%1").arg(i), 0).toInt(), -32768, 32767));
        out.VerticalSpacing[i] = static_cast<int16_t>(clampInt(settings.value(QString("verticalSpacing%1").arg(i), 0).toInt(), -32768, 32767));
    }
    out.HorizontalAlignReference = static_cast<unsigned char>(clampInt(settings.value("horizontalAlignReference", 0).toInt(), 0, 255));
    out.VerticalAlignReference = static_cast<unsigned char>(clampInt(settings.value("verticalAlignReference", 0).toInt(), 0, 255));
    variantListToByteArray(settings.value("leftChannelH1"), out.LeftChannelAlign_H1, 8);
    variantListToByteArray(settings.value("leftChannelH2"), out.LeftChannelAlign_H2, 8);
    variantListToByteArray(settings.value("leftChannelH3"), out.LeftChannelAlign_H3, 8);
    variantListToByteArray(settings.value("leftChannelH4"), out.LeftChannelAlign_H4, 8);
    variantListToByteArray(settings.value("rightChannelH1"), out.RightChannelAlign_H1, 8);
    variantListToByteArray(settings.value("rightChannelH2"), out.RightChannelAlign_H2, 8);
    variantListToByteArray(settings.value("rightChannelH3"), out.RightChannelAlign_H3, 8);
    variantListToByteArray(settings.value("rightChannelH4"), out.RightChannelAlign_H4, 8);
    return out;
}

QVariantMap NocaiDirectPrintClient::uvParamValuesToMap(const UVParamValues& values) const
{
    QVariantMap out;
    out["rightR2LOffset"] = values.RightR2LOffset;
    out["rightL2ROffset"] = values.RightL2ROffset;
    out["leftR2LOffset"] = values.LeftR2LOffset;
    out["leftL2ROffset"] = values.LeftL2ROffset;
    out["lampL2ROffset"] = values.LampL2ROffset;
    return out;
}

NocaiDirectPrintClient::UVParamValues
NocaiDirectPrintClient::uvParamValuesFromMap(const QVariantMap& settings) const
{
    UVParamValues out;
    auto s16 = [&](const char* key) {
        return static_cast<int16_t>(clampInt(settings.value(QString::fromUtf8(key), 0).toInt(), -32768, 32767));
    };
    out.RightR2LOffset = s16("rightR2LOffset");
    out.RightL2ROffset = s16("rightL2ROffset");
    out.LeftR2LOffset = s16("leftR2LOffset");
    out.LeftL2ROffset = s16("leftL2ROffset");
    out.LampL2ROffset = s16("lampL2ROffset");
    return out;
}

QVariantMap NocaiDirectPrintClient::newUvParamValuesToMap(const NewUVParamValues& values) const
{
    QVariantMap out;
    out["leftStartOffset"] = values.UVLampLeftStartOffset;
    out["leftEndOffset"] = values.UVLampLeftEndOffset;
    out["leftMinOffset"] = values.UVLampLeftMinOffset;
    out["rightStartOffset"] = values.UVLampRightStartOffset;
    out["rightEndOffset"] = values.UVLampRightEndOffset;
    out["rightMinOffset"] = values.UVLampRightMinOffset;
    out["delayDistance"] = values.UVLampDelayDistance;
    return out;
}

NocaiDirectPrintClient::NewUVParamValues
NocaiDirectPrintClient::newUvParamValuesFromMap(const QVariantMap& settings) const
{
    NewUVParamValues out;
    auto s16 = [&](const char* key) {
        return static_cast<int16_t>(clampInt(settings.value(QString::fromUtf8(key), 0).toInt(), -32768, 32767));
    };
    out.UVLampLeftStartOffset = s16("leftStartOffset");
    out.UVLampLeftEndOffset = s16("leftEndOffset");
    out.UVLampLeftMinOffset = s16("leftMinOffset");
    out.UVLampRightStartOffset = s16("rightStartOffset");
    out.UVLampRightEndOffset = s16("rightEndOffset");
    out.UVLampRightMinOffset = s16("rightMinOffset");
    out.UVLampDelayDistance = s16("delayDistance");
    return out;
}

bool NocaiDirectPrintClient::withSdkWorkingDirectory(const std::function<bool()>& callback)
{
    if (m_resolvedSdkRoot.isEmpty()) {
        setError("Direct print SDK root is not resolved.");
        return false;
    }

    ScopedCurrentDir scoped(m_resolvedSdkRoot);
    if (!scoped.changed) {
        setError(QString("Failed to enter direct print SDK folder: %1").arg(m_resolvedSdkRoot));
        return false;
    }

    return callback();
}

NocaiDirectPrintClient::JobSettings
NocaiDirectPrintClient::makeJobSettings(const NocaiDirectPrintSettings& settings) const
{
    JobSettings out;
    out.PrintDirection = static_cast<uint16_t>(clampInt(settings.printDirection, 0, 3));
    out.PrintSpeed = static_cast<uint16_t>(clampInt(settings.printSpeed, 0, 3));
    out.WCSequence = static_cast<uint16_t>(clampInt(settings.wcSequence, 0, 1));
    out.EclosionGrade = static_cast<uint16_t>(clampInt(settings.eclosionGrade, 0, 3));
    out.HeadSelect = static_cast<uint16_t>(clampInt(settings.headSelect, 0, 2));
    out.WInkPercent = static_cast<uint16_t>(clampInt(settings.whiteInkPercent, 0, 9));
    out.WInkPassCount = static_cast<uint16_t>(clampInt(settings.whiteInkPassCount, 0, 255));
    out.VInkPercent = static_cast<uint16_t>(clampInt(settings.varnishInkPercent, 0, 9));
    out.VInkPassCount = static_cast<uint16_t>(clampInt(settings.varnishInkPassCount, 0, 255));
    out.HeadVoltage = static_cast<uint16_t>(clampInt(settings.headVoltage, 400, 600));
    out.DisableUVLights[0] = static_cast<unsigned char>(clampInt(settings.disableUv0, 0, 1));
    out.DisableUVLights[1] = static_cast<unsigned char>(clampInt(settings.disableUv1, 0, 1));
    out.DisableUVLights[2] = static_cast<unsigned char>(clampInt(settings.disableUv2, 0, 1));
    out.DisableUVLights[3] = static_cast<unsigned char>(clampInt(settings.disableUv3, 0, 1));
    out.DisableUVLights[4] = static_cast<unsigned char>(clampInt(settings.disableUv4, 0, 1));
    out.DisableUVLights[5] = static_cast<unsigned char>(clampInt(settings.disableUv5, 0, 1));
    out.CarReset = static_cast<uint16_t>(clampInt(settings.carReset, 0, 1));
    out.stripBlank = static_cast<uint16_t>(clampInt(settings.stripBlank, 0, 2));
    out.blankDistance = static_cast<uint16_t>(clampInt(settings.blankDistance, 0, 65535));
    return out;
}
