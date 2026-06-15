// PrintJobNocai.h

#include <QObject>
#include <QString>
#include <QStringList>
#include <QStandardPaths>
#include <QtConcurrent>
#include <QTemporaryDir>
#include <QVariantMap>
#include <QVariantList>
#include <QList>
#include <QPair>
#include <array>
#include <vector>
#include <cstdint>
#include <memory>
#include <Magick++.h>
#include "ColorManagementManager.h"

// PRN generation pipeline: input load -> optional ICC convert -> CMYK separation ->
// blue-noise thresholding -> dot classification (2bpp) -> PRN write. Exposed to QML.

class ColorManagementManager;

// Ink dot strategy (thresholds and optional neighborhood promotion).
struct DotStrategy {
	int minInkThreshold = 8;		// Below this (channel < min), skip Ink dotting in FM gate
	int smallDotThreshold = 104;	// Base cut for SMALL after tone-normalization (tRel <= smallCut == small dot(1)
	int medDotThreshold = 168;		// Base cut for MEDIUM (smallCut < tRel <= medCut == medium dot(2)
									// Everything else == large dot (3)
	bool enablePromotion = false;	// Optional neighborhood-based upsize in dense areas
	
    // Floor gating for FM screening.
	uint8_t floorRangeCMY = 24;		// FM Screening Range for CMY
    uint8_t floorMaxCMY   = 2;		// FM Screening Max for CMY
    uint8_t floorRangeK   = 12;		// FM Screening Range for K
    uint8_t floorMaxK     = 0;		// FM Screening Max for K
    
    bool enableDotSwap = false;		// Optional, allows swapping Small and Large Ink Dots
};


class PrintJobNocai : public QObject {
    Q_OBJECT

signals:
    void prnGenerationFinished(bool success);	// Emitted when runPRNGeneration completes.

public slots:
    Q_INVOKABLE void runPRNGeneration(const QVariantMap& jobMap, const QString& outputPath);		// End-to-end async entry.

public:
    explicit PrintJobNocai(QObject* parent = nullptr);
    
    void setColorManager(ColorManagementManager* mgr);

    // QML-exposed pipeline
    Q_INVOKABLE bool loadInputImage(const QString& imagePath);										// Read + stage RGB/CMYK.
    Q_INVOKABLE bool applyICCConversion(const QString& inputProfile, const QString& outputProfile);	// Input->output ICC.
    Q_INVOKABLE bool generateFinalPRN(const QString& outputPath, int xdpi, int ydpi);				// Threshold, pack, write.
    
    // Internal Assets Handling for Blue Noise Mask and ICC Profiles
    Q_INVOKABLE void prepareNocaiAssets();															// Extract/copy assets to temp.
    Q_INVOKABLE void cleanupTemporaryFiles(const QString& baseName, const QString& workingDir);		// Remove temp by base.
    Q_INVOKABLE void cleanupRuntimeAssets();														// Tear down temp dir, etc.
    
    // ICC Profile Handling
    Q_INVOKABLE QVariantList getAvailableICCProfiles() const;										// [{name, path}, ...]
    Q_INVOKABLE QString getDefaultOutputICCProfile() const;											// Default out profile path.
    Q_INVOKABLE QString getDefaultInputCMYKProfile() const;											// Default input CMYK path.
    Q_INVOKABLE void setDefaultOutputICCProfile(const QString& outputProfile);						// Set Default input ICC path.
    Q_INVOKABLE void setDefaultInputCMYKProfile(const QString& inputProfilePath);					// Set Default input CMYK path.
    Q_INVOKABLE void enableDefaultInputCMYK(bool enabled);   										// Global toggle (can be overridden per jobMap)
    Q_INVOKABLE bool checkDefaultInputCMYK() const;													
    Q_INVOKABLE void addICCProfile(const QString& name, const QString& path);						// Add ICC Profile to available list.

	// Ink dot thresholds (single call to set all).
	Q_INVOKABLE void setDotStrategy(int minInkThreshold, int smallDotThreshold, int medDotThreshold, bool enablePromotion, uint8_t floorRangeCMY, uint8_t floorMaxCMY, uint8_t floorRangeK, uint8_t floorMaxK, bool enableDotSwap);


private:

	ColorManagementManager* m_colorManager = nullptr;

    // Working images and intermediate data.
    Magick::Image inputImage;                        	// RGB input (temporary copy)
    std::array<Magick::Image, 4> cmykChannels;       	// C, M, Y, K separated
    std::array<Magick::Image, 4> thresholdMasks;     	// Blue noise masks per channel
    std::array<std::vector<uint8_t>, 4> dotMaps;     	// Per-pixel dot class (0..3)
    std::array<std::vector<uint8_t>, 4> packedOutput;	// 2bpp packed lines per channel
    std::array<Magick::Image, 4> separateCMYK(Magick::Image& cmyk);

    // Paths and temp handling
    QString assetsExtractPath;
    QString originalFilename;
    QString tempImagePath;
	bool assetsPrepared = false;
    std::unique_ptr<QTemporaryDir> tempDir;
    
    // ICC profile state.
    Magick::Blob loadICCProfile(const QString& filePath);
    QString defaultOutputICCPath;
	QString defaultInputCMYKPath;
    bool useDefaultInputCMYK = true;
    QList<QPair<QString, QString>> availableICCProfiles; // name, path

  	// Screening/packing parameters.
   	DotStrategy dotStrategy;
   	uint32_t screenSeed = 0;  // seed the mask phase per run
    std::vector<std::vector<uint8_t>> dotClassification(const std::vector<uint8_t>& dithered, const std::vector<uint8_t>& mask, const std::vector<uint8_t>& channel, int width, int height, const DotStrategy& strategy);
    
    // void apply4x4Promotion(std::vector<std::vector<uint8_t>>& dotMap, int width, int height);
    void apply4x4Promotion(std::vector<std::vector<uint8_t>>& dotMap, const std::vector<uint8_t>& tone, int width, int height);

	// PRN Helper Functions    
    std::vector<std::vector<uint8_t>> packTo2BPP(const std::vector<std::vector<uint8_t>>& dotMap, int width, int height);
    bool writePRNFile(const std::vector<std::vector<std::vector<uint8_t>>>& packedLines, const std::vector<int>& channelOrder, int width, int height, int xdpi, int ydpi, const QString& outputPath);

};
