#pragma once

#include <QObject>
#include <QVariantList>
#include <QString>
#include <QStringList>
#include <QLibrary>
#include <QRecursiveMutex>

#include <cstdint>
#include <functional>
#include <type_traits>
#include <vector>

struct NocaiDirectPrintSettings
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

struct NocaiDirectPrintRaster
{
    const std::vector<std::vector<std::vector<uint8_t>>>* packedLines = nullptr;
    std::vector<int> channelOrder;
    int width = 0;
    int height = 0;
    int xdpi = 0;
    int ydpi = 0;
    int bytesPerLine = 0;
};

class NocaiDirectPrintClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ isAvailable NOTIFY statusChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY statusChanged)
    Q_PROPERTY(QString sdkRootPath READ sdkRootPath WRITE setSdkRootPath NOTIFY statusChanged)
    Q_PROPERTY(QVariantList printers READ printers NOTIFY printersChanged)
    Q_PROPERTY(QStringList maintenanceSupportedPrinters READ maintenanceSupportedPrinters CONSTANT)

public:
    explicit NocaiDirectPrintClient(QObject* parent = nullptr);

    bool isAvailable();
    QString lastError() const;
    QString sdkRootPath() const;
    void setSdkRootPath(const QString& path);
    QVariantList printers() const;
    QStringList maintenanceSupportedPrinters() const;

    Q_INVOKABLE QVariantList searchPrinters();
    Q_INVOKABLE bool supportsMaintenance(const QString& printerName) const;
    Q_INVOKABLE bool refreshPrinters();
    Q_INVOKABLE bool choosePrinter(int index);
    Q_INVOKABLE bool abortPrint();
    Q_INVOKABLE bool pausePrint();
    Q_INVOKABLE bool continuePrint();
    Q_INVOKABLE QString statusText();
    Q_INVOKABLE bool connectPrinter();
    Q_INVOKABLE bool wipePrintHead(int printHeadMask);
    Q_INVOKABLE bool startCleanOperation(int printHeadMask);
    Q_INVOKABLE bool startPump(int printHeadMask);
    Q_INVOKABLE bool stopPumpOperation();
    Q_INVOKABLE bool spitPrintHead(int printHeadMask);
    Q_INVOKABLE bool stopSpitOperation();
    Q_INVOKABLE bool capPrintHead();
    Q_INVOKABLE bool moveAxis(int axis, int direction);
    Q_INVOKABLE QVariantMap stopAxis(int axis);
    Q_INVOKABLE QVariantMap saveAxisPos(int axis);
    Q_INVOKABLE bool setPrintHeight(int heightMm);
    Q_INVOKABLE QVariantMap getPrintHeight();
    Q_INVOKABLE QVariantMap getJobSettings();
    Q_INVOKABLE bool setJobSettingsFromMap(const QVariantMap& settings);
    Q_INVOKABLE bool exportConfigFile(const QString& path);
    Q_INVOKABLE bool importConfigFile(const QString& path);
    Q_INVOKABLE QVariantMap getAlignmentValues();
    Q_INVOKABLE bool setAlignmentValues(const QVariantMap& settings, int type);
    Q_INVOKABLE bool printAlignmentPattern(int type);
    Q_INVOKABLE QVariantMap getPrinterStatus();
    Q_INVOKABLE QVariantMap getPrinterInfo();
    Q_INVOKABLE bool setPrintXYValue(int xMm, int yMm);
    Q_INVOKABLE QVariantMap getPrintXYValue();
    Q_INVOKABLE QVariantMap getUVParamValues();
    Q_INVOKABLE bool setUVParamValues(const QVariantMap& settings, int type);
    Q_INVOKABLE int getSupportNewUVParamFunction();
    Q_INVOKABLE bool setNewUVParamFunction(int type);
    Q_INVOKABLE QVariantMap getNewUVParamValues();
    Q_INVOKABLE bool setNewUVParamValues(const QVariantMap& settings, int type);

    bool printPackedJob(const NocaiDirectPrintRaster& raster,
                        const NocaiDirectPrintSettings& settings);

signals:
    void statusChanged();
    void printersChanged();

private:
    struct PrinterInfoList;
    struct PrintJobProperty;
    struct JobSettings;
    struct AlignmentValues;
    struct PrinterStatus;
    struct PrinterInfo;
    struct UVParamValues;
    struct NewUVParamValues;

    using SearchPrinterFn = int (*)(PrinterInfoList*, int);
    using ChoosePrinterFn = int (*)(int);
    using ContinuePrintFn = int (*)();
    using InitPrinterFn = int (*)();
    using StartPrintFn = int (*)(PrintJobProperty*);
    using PrintALineFn = int (*)(char*, uint32_t);
    using AbortPrintFn = int (*)();
    using PausePrintFn = int (*)();
    using EndPrintFn = int (*)();
    using ClosePrintFn = int (*)();
    using SetJobSettingsFn = int (*)(JobSettings*, int);
    using GetJobSettingsFn = int (*)(JobSettings*, int);
    using ConnectPrinterFn = int (*)();
    using HeadMaskFn = int (*)(int);
    using NoArgFn = int (*)();
    using MoveAxisFn = int (*)(int, int);
    using AxisPosFn = int (*)(int, int*);
    using SetPrintHeightFn = int (*)(uint16_t);
    using GetPrintHeightFn = int (*)(uint16_t*);
    using SetAlignmentValuesFn = int (*)(AlignmentValues*, int, int);
    using GetAlignmentValuesFn = int (*)(AlignmentValues*, int);
    using ConfigFileFn = int (*)(char*);
    using PrintAlignmentPatternFn = int (*)(int);
    using GetPrinterStatusFn = int (*)(PrinterStatus*, int);
    using GetPrinterInfoFn = int (*)(PrinterInfo*, int);
    using SetPrintXYValueFn = int (*)(uint32_t, uint32_t);
    using GetPrintXYValueFn = int (*)(uint32_t*, uint32_t*);
    using SetUVParamValuesFn = int (*)(UVParamValues*, int, int);
    using GetUVParamValuesFn = int (*)(UVParamValues*, int);
    using GetSupportNewUVParamFunctionFn = int (*)();
    using SetNewUVParamFunctionFn = int (*)(int);
    using SetNewUVParamValuesFn = int (*)(NewUVParamValues*, int, int);
    using GetNewUVParamValuesFn = int (*)(NewUVParamValues*, int);

    bool ensureLoaded();
    bool resolveSymbols();
    QString resolveSdkRoot() const;
    QStringList sdkRootCandidates() const;
    void setError(const QString& message);
    bool callSucceeded(int result, const QString& functionName);
    bool requireFunction(const void* fn, const QString& functionName);
    bool withSdkWorkingDirectory(const std::function<bool()>& callback);
    JobSettings makeJobSettings(const NocaiDirectPrintSettings& settings) const;
    QVariantMap jobSettingsToMap(const JobSettings& settings) const;
    JobSettings jobSettingsFromMap(const QVariantMap& settings) const;
    QVariantMap alignmentValuesToMap(const AlignmentValues& values) const;
    AlignmentValues alignmentValuesFromMap(const QVariantMap& settings) const;
    QVariantMap uvParamValuesToMap(const UVParamValues& values) const;
    UVParamValues uvParamValuesFromMap(const QVariantMap& settings) const;
    QVariantMap newUvParamValuesToMap(const NewUVParamValues& values) const;
    NewUVParamValues newUvParamValuesFromMap(const QVariantMap& settings) const;

    mutable QRecursiveMutex m_mutex;
    QLibrary m_library;
    QString m_sdkRootPath;
    QString m_resolvedSdkRoot;
    QString m_lastError;
    QVariantList m_printers;

    SearchPrinterFn m_searchPrinter = nullptr;
    ChoosePrinterFn m_choosePrinter = nullptr;
    ContinuePrintFn m_continuePrint = nullptr;
    InitPrinterFn m_initPrinter = nullptr;
    StartPrintFn m_startPrint = nullptr;
    PrintALineFn m_printALine = nullptr;
    AbortPrintFn m_abortPrint = nullptr;
    PausePrintFn m_pausePrint = nullptr;
    EndPrintFn m_endPrint = nullptr;
    ClosePrintFn m_closePrint = nullptr;
    SetJobSettingsFn m_setJobSettings = nullptr;
    GetJobSettingsFn m_getJobSettings = nullptr;
    ConnectPrinterFn m_connectPrinter = nullptr;
    HeadMaskFn m_wipePrintHead = nullptr;
    HeadMaskFn m_startCleanOperation = nullptr;
    HeadMaskFn m_startPump = nullptr;
    NoArgFn m_stopPumpOperation = nullptr;
    HeadMaskFn m_spitPrintHead = nullptr;
    NoArgFn m_stopSpitOperation = nullptr;
    NoArgFn m_capPrintHead = nullptr;
    MoveAxisFn m_moveAxis = nullptr;
    AxisPosFn m_stopAxis = nullptr;
    AxisPosFn m_saveAxisPos = nullptr;
    SetPrintHeightFn m_setPrintHeight = nullptr;
    GetPrintHeightFn m_getPrintHeight = nullptr;
    SetAlignmentValuesFn m_setAlignmentValues = nullptr;
    GetAlignmentValuesFn m_getAlignmentValues = nullptr;
    ConfigFileFn m_exportConfigFile = nullptr;
    ConfigFileFn m_importConfigFile = nullptr;
    PrintAlignmentPatternFn m_printAlignmentPattern = nullptr;
    GetPrinterStatusFn m_getPrinterStatus = nullptr;
    GetPrinterInfoFn m_getPrinterInfo = nullptr;
    SetPrintXYValueFn m_setPrintXYValue = nullptr;
    GetPrintXYValueFn m_getPrintXYValue = nullptr;
    SetUVParamValuesFn m_setUVParamValues = nullptr;
    GetUVParamValuesFn m_getUVParamValues = nullptr;
    GetSupportNewUVParamFunctionFn m_getSupportNewUVParamFunction = nullptr;
    SetNewUVParamFunctionFn m_setNewUVParamFunction = nullptr;
    SetNewUVParamValuesFn m_setNewUVParamValues = nullptr;
    GetNewUVParamValuesFn m_getNewUVParamValues = nullptr;
};
