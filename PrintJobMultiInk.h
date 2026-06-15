#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QTemporaryDir>
#include <QList>
#include <QPair>

#include <array>
#include <vector>
#include <cstdint>
#include <memory>

#include <Magick++.h>
#include <lcms2.h>

#include "AssetManager.h"
#include "ColorManagementManager.h"
#include "MultiInkLinearization.h"
#include "MultiInkToneBuilder.h"
#include "MultiInkScreenEngine.h"
#include "MultiInkTypes.h"

class ColorManagementManager;

class PrintJobMultiInk : public QObject
{
    Q_OBJECT

public:
    enum class InkMode {
        FourColor_YMCK = 4,                    // YMCK
        FiveColor_YMCK_W = 5,                  // YMCK + White
        SixColor_YMCK_Lm_Lc = 6,               // YMCK + Lm + Lc
        SevenColor_YMCK_Lm_Lc_W = 7,           // YMCK + Lm + Lc + W
        EightColor_YMCK_Lm_Lc_Lk_LLk = 8,      // YMCK + Lm + Lc + Lk + LLk
        TenColor_YMCK_Lm_Lc_Lk_LLk_W_V = 10    // YMCK + Lm + Lc + Lk + LLk + W + V
    };
    Q_ENUM(InkMode)

    explicit PrintJobMultiInk(QObject* parent = nullptr);

signals:
    void prnGenerationFinished(bool success);

public slots:
    Q_INVOKABLE void runPRNGeneration(const QVariantMap& jobMap, const QString& outputPath);

public:
    // Manager wiring
    void setColorManager(ColorManagementManager* mgr);

    // Pipeline entry points
    Q_INVOKABLE bool loadInputImage(const QString& imagePath);
    Q_INVOKABLE bool applyICCConversion(const QString& inputProfile, const QString& outputProfile);
    Q_INVOKABLE bool generateFinalPRN(const QString& outputPath, int xdpi, int ydpi);

    // Ink mode
    Q_INVOKABLE void setInkMode(int mode);
    Q_INVOKABLE int inkMode() const;

    // Assets / cleanup
    Q_INVOKABLE bool prepareAssets();
    Q_INVOKABLE bool cleanupAssets();
    Q_INVOKABLE void cleanupTemporaryFiles(const QString& baseName, const QString& workingDir);

    // DeviceLink
    Q_INVOKABLE void enableDeviceLink(bool enabled);
    Q_INVOKABLE bool isDeviceLinkEnabled() const;
    Q_INVOKABLE void setDefaultDeviceLinkProfile(const QString& path);
    Q_INVOKABLE QString getDefaultDeviceLinkProfile() const;
    Q_INVOKABLE void addDeviceLinkProfile(const QString& name, const QString& path);
    Q_INVOKABLE QVariantList getAvailableDeviceLinkProfiles() const;

    // ICC profiles
    Q_INVOKABLE QVariantList getAvailableICCProfiles() const;
    Q_INVOKABLE QString getDefaultOutputICCProfile() const;
    Q_INVOKABLE QString getDefaultInputCMYKProfile() const;
    Q_INVOKABLE void setDefaultOutputICCProfile(const QString& outputProfile);
    Q_INVOKABLE void setDefaultInputCMYKProfile(const QString& inputProfilePath);
    Q_INVOKABLE void enableDefaultInputCMYK(bool enabled);
    Q_INVOKABLE bool checkDefaultInputCMYK() const;
    Q_INVOKABLE void addICCProfile(const QString& name, const QString& path);

    // Dot strategy
    Q_INVOKABLE void setDotStrategy(int minInkThreshold,
                                    int smallDotThreshold,
                                    int medDotThreshold,
                                    bool enablePromotion,
                                    uint8_t floorRangeCMY,
                                    uint8_t floorMaxCMY,
                                    uint8_t floorRangeK,
                                    uint8_t floorMaxK,
                                    bool enableDotSwap);

    // Linearization
    Q_INVOKABLE void enableLinearization(bool enabled);
    Q_INVOKABLE bool isLinearizationEnabled() const;

private:
    // Internal helpers
    bool loadMaskRaw(const QString& key,
                     std::vector<uint8_t>& maskRaw,
                     int& maskW,
                     int& maskH) const;

    bool loadExternalPlateTone(const QString& platePath,
                               std::vector<uint8_t>& outTone,
                               int width,
                               int height) const;

    bool applyDeviceLinkConversion(const QString& deviceLinkPath);
    bool reloadLinearizationFromManager();

    std::array<Magick::Image, 4> separateCMYK(Magick::Image& cmykImage);
    QString maskKeyForChannel(InkMode mode, int channelIndex) const;

    bool writePRNFile(const std::vector<std::vector<std::vector<uint8_t>>>& packedLines,
                      const std::vector<int>& channelOrder,
                      int width,
                      int height,
                      int xdpi,
                      int ydpi,
                      const QString& outputPath);

private:
    // Runtime mode/config
    QVariantMap m_modeParams;
    InkMode m_inkMode = InkMode::FourColor_YMCK;
    ColorManagementManager* m_colorManager = nullptr;

    // Assets
    AssetManager m_assetManager;
    bool m_assetsPrepared = false;

    // Working images / paths
    Magick::Image inputImage;
    QString originalFilename;
    QString tempImagePath;
    std::unique_ptr<QTemporaryDir> tempDir;
    QString m_whitePlatePath;
    QString m_varnishPlatePath;

    // ICC state
    QString defaultOutputICCPath;
    QString defaultInputCMYKPath;
    bool useDefaultInputCMYK = false;
    QList<QPair<QString, QString>> availableICCProfiles;

    // DeviceLink state
    bool m_useDeviceLink = false;
    QString m_defaultDeviceLinkPath;
    QList<QPair<QString, QString>> m_availableDeviceLinkProfiles;

    // Linearization
    MultiInkLinearization m_linearization;
    bool m_enableLinearization = true;

    // Screening state
    MultiInkDotStrategy dotStrategy;
    uint32_t screenSeed = 0;
};
