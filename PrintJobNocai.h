#include <QObject>
#include <QString>
#include <QStringList>
#include <QStandardPaths>
#include <QtConcurrent>
#include <QTemporaryDir>
#include <array>
#include <Magick++.h>


// Ink Dot Strategy
struct DotStrategy {
	int minInkThreshold = 2;         // Below this: no dot
	int smallDotThreshold = 192;     // ≥ this → small dot (1)
	int medDotThreshold = 128;       // ≥ this → medium dot (2)
	                                 // ≥ minInkThreshold and < medDotThreshold → large dot (3)
	bool enablePromotion = true;
};


class PrintJobNocai : public QObject {
    Q_OBJECT

signals:
    void prnGenerationFinished(bool success);

public slots:
    Q_INVOKABLE void runPRNGeneration(const QVariantMap& jobMap, const QString& outputPath);

public:
    explicit PrintJobNocai(QObject* parent = nullptr);

    // QML-exposed pipeline
    Q_INVOKABLE bool loadInputImage(const QString& imagePath);
    Q_INVOKABLE bool applyICCConversion(const QString& inputProfile, const QString& outputProfile);
    Q_INVOKABLE bool generateFinalPRN(const QString& outputPath, int xdpi, int ydpi);
    
    // Internal Assets Handling for Blue Noise Mask and ICC Profiles
    Q_INVOKABLE void prepareNocaiAssets();
    Q_INVOKABLE void cleanupTemporaryFiles(const QString& baseName, const QString& workingDir);
    Q_INVOKABLE void cleanupRuntimeAssets();
    
    // ICC Profile Handling
    Q_INVOKABLE QVariantList getAvailableICCProfiles() const;
    Q_INVOKABLE QString getDefaultOutputICCProfile() const;
    Q_INVOKABLE void setDefaultOutputICCProfile(const QString& outputProfile);
    Q_INVOKABLE void addICCProfile(const QString& name, const QString& path);

	// Ink Dot Thresholds
	Q_INVOKABLE void setDotStrategy(int minInkThreshold, int smallDotThreshold, int medDotThreshold, bool enablePromotion);


private:

    // Internal images and data
    Magick::Image inputImage;                        // RGB input (temporary copy)
    std::array<Magick::Image, 4> cmykChannels;       // C, M, Y, K separated
    std::array<Magick::Image, 4> thresholdMasks;     // Blue noise masks per channel
    std::array<std::vector<uint8_t>, 4> dotMaps;     // Dot size maps per channel
    std::array<std::vector<uint8_t>, 4> packedOutput;// 2BPP output per channel
    std::array<Magick::Image, 4> separateCMYK(Magick::Image& cmyk);
    Magick::Image buildDitherMask(const Magick::Image& baseMask, int width, int height, int offsetX, int offsetY);

    // Paths and temp handling
    QString assetsExtractPath;
    QString originalFilename;
    QString tempImagePath;
    std::unique_ptr<QTemporaryDir> tempDir;
    
    // Internal helpers
    Magick::Blob loadICCProfile(const QString& filePath);
    QString defaultOutputICCPath;
    QList<QPair<QString, QString>> availableICCProfiles; // name, path

    // PRN Generation Helper Functions
   	DotStrategy dotStrategy;
    std::vector<std::vector<uint8_t>> dotClassification(const std::vector<uint8_t>& dithered, const std::vector<uint8_t>& mask, int width, int height, const DotStrategy& strategy);
    void apply4x4Promotion(std::vector<std::vector<uint8_t>>& dotMap, int width, int height);
    std::vector<std::vector<uint8_t>> packTo2BPP(const std::vector<std::vector<uint8_t>>& dotMap, int width, int height);
    bool writePRNFile(const std::vector<std::vector<std::vector<uint8_t>>>& packedLines, const std::vector<int>& channelOrder, int width, int height, int xdpi, int ydpi, const QString& outputPath);

};
