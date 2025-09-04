#include "PrintJobNocai.h"
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QStandardPaths>
#include <QDirIterator>
#include <QFileInfo>
#include <QDateTime>
#include <QProcess>
#include <QDebug>
#include <QUrl>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>


/* PrintJobNocai.cpp
 * End-to-end PRN pipeline:
 *   loadInputImage -> (optional) applyICCConversion -> generateFinalPRN
 * Core stages (in generateFinalPRN):
 *   1) DPI scaling
 *   2) CMYK channel separation
 *   3) Blue-noise thresholding (FM screen) with highlight floor gating
 *   4) Dot classification (small/medium/large) using mask-relative thresholds
 *   5) Optional neighborhood-based “promotion” to reduce peppering
 *   6) 2bpp packing per channel and PRN write with simple header
 */
 
 
// Constructor; No-operation; state set by loader and setters.
PrintJobNocai::PrintJobNocai(QObject* parent) : QObject(parent) {}


// Per-pixel hash used to derive stable channel-specific phase and probabilistic swaps.
static inline uint32_t pxhash(uint32_t x, uint32_t y, uint32_t ch, uint32_t seed=0x9E3779B9u) {
    uint32_t h = x * 0x85EBCA6Bu ^ y * 0xC2B2AE35u ^ (ch+1) * 0x27D4EB2Du ^ seed;
    h ^= h >> 16; h *= 0x7FEB352Du; h ^= h >> 15; h *= 0x846CA68Bu; h ^= h >> 16;
    return h;
}


// 8-bit linear interpolation helper: w in [0..255].
static inline uint8_t lerp_u8(uint8_t a, uint8_t b, uint8_t w /*0..255*/) {
    // (1 - w/255) * a + (w/255) * b
    return static_cast<uint8_t>(((255 - w) * a + w * b) / 255);
}


/* === Async entry point from QML ===
 * - Sets dot thresholds from jobMap
 * - Loads image
 * - Applies ICC conversion (sRGB->printer if RGB; CMYK->printer if enabled)
 * - Seeds screening
 * - Calls generateFinalPRN and emits prnGenerationFinished
 */
void PrintJobNocai::runPRNGeneration(const QVariantMap& jobMap, const QString& outputPath) {
    (void) QtConcurrent::run([=]() {
        bool success = false;
        
        const QString imagePath 	= jobMap["imagePath"].toString();
        const QSize resolution 		= jobMap["resolution"].toSize();
        const QString colorProfile 	= jobMap["colorProfile"].toString();
        const int minThreshold 		= jobMap["minInkThreshold"].toInt();
		const int smallThreshold 	= jobMap["smallDotThreshold"].toInt();
		const int medThreshold 		= jobMap["medDotThreshold"].toInt();
		const bool promotionEnabled = jobMap["enablePromotion"].toBool();
		const int  floorRangeCMY    = jobMap.value("floorRangeCMY", 24).toInt();
		const int  floorMaxCMY      = jobMap.value("floorMaxCMY",   2).toInt();
		const int  floorRangeK      = jobMap.value("floorRangeK",  12).toInt();
		const int  floorMaxK        = jobMap.value("floorMaxK",     0).toInt();
		const bool enableDotSwap    = jobMap.value("enableDotSwap", false).toBool();

		setDotStrategy(minThreshold,
               smallThreshold,
               medThreshold,
               promotionEnabled,
               static_cast<uint8_t>(floorRangeCMY),
               static_cast<uint8_t>(floorMaxCMY),
               static_cast<uint8_t>(floorRangeK),
               static_cast<uint8_t>(floorMaxK),
               enableDotSwap
        );

		qDebug() << "Dot strategy updated:"
				 << "MinInk:" << dotStrategy.minInkThreshold
				 << "SmallCut:" << dotStrategy.smallDotThreshold
				 << "MedCut:" << dotStrategy.medDotThreshold
				 << "Promotion:" << dotStrategy.enablePromotion
				 << "SwapSmallLarge:" << dotStrategy.enableDotSwap
				 << "Floor(CMY):" << dotStrategy.floorRangeCMY << "/" << dotStrategy.floorMaxCMY
				 << "Floor(K):"   << dotStrategy.floorRangeK   << "/" << dotStrategy.floorMaxK;

        if (loadInputImage(imagePath)) {
            // Convert to printer CMYK as needed.
            if (inputImage.colorSpace() != Magick::CMYKColorspace) {
                qDebug() << "Input is NOT CMYK — applying ICC conversion (sRGB → RIP CMYK)";

                QString inputICC = assetsExtractPath + "/sRGBProfile.icm";
                QString outputICC = defaultOutputICCPath;

                success = applyICCConversion(inputICC, outputICC);
                
             } else {
                // CMYK source → optionally CMYK -> Printer CMYK using global toggle/path
                if (useDefaultInputCMYK && !defaultInputCMYKPath.isEmpty()) {
                    qDebug() << "Input is CMYK — applying ICC conversion (Default CMYK Input → RIP CMYK)";
                    const QString inputICC  = defaultInputCMYKPath;
                    const QString outputICC = defaultOutputICCPath;
                    success = applyICCConversion(inputICC, outputICC);
                } else {
                    qDebug() << "Input is CMYK, default CMYK input profile disabled — skipping ICC conversion.";
                    success = true; // Default CMYK Profile Off
                }
            }
            
            // Stable-but-changing screen seed based on path and time.
			screenSeed = qHash(imagePath) ^ static_cast<uint32_t>(QDateTime::currentMSecsSinceEpoch() & 0xFFFFFFFF);

            if (success) {
                const int xdpi = resolution.width();
                const int ydpi = resolution.height();
                success = generateFinalPRN(outputPath, xdpi, ydpi);
            }
        }
        emit prnGenerationFinished(success);
    });
}


// Main PRN generation: separation -> threshold -> classify -> promote -> pack -> write.
bool PrintJobNocai::generateFinalPRN(const QString& outputPath, int xdpi, int ydpi) {
    try {
        const std::vector<int> nocaiOrder = {2, 1, 0, 3};	// Output channel order: Y, M, C, K.
        const QStringList chKeys = {"c", "m", "y", "k"};

        if (inputImage.colorSpace() != Magick::CMYKColorspace) {
            qWarning() << "Input image is not in CMYK colorspace.";
            return false;
        }
        
        // === Step 1: DPI scaling relative to 720 baseline.
      	double scaleX = static_cast<double>(xdpi) / 720.0;
        double scaleY = static_cast<double>(ydpi) / 720.0;

        if (scaleX != 1.0 || scaleY != 1.0) {
            int newWidth = static_cast<int>(inputImage.columns() * scaleX);
            int newHeight = static_cast<int>(inputImage.rows() * scaleY);

			// Apply Resize to fit output DPI
			QString resizeGeometry = QString("%1x%2!").arg(newWidth).arg(newHeight);
			inputImage.resize(Magick::Geometry(resizeGeometry.toStdString()));

            qDebug() << "Rescaled image for output DPI:" << xdpi << "x" << ydpi << "→" << inputImage.columns() << "x" << inputImage.rows();
        }
        
        // === Step 2: Separate into CMYK channels
		auto cmykChannels = separateCMYK(inputImage);
        int width = static_cast<int>(cmykChannels[0].columns());
        int height = static_cast<int>(cmykChannels[0].rows());

        // === Step 3: Prepare mask file paths (blue noise masks live in assetsExtractPath).
        std::array<QString, 4> maskPaths;
        for (int i = 0; i < 4; ++i)
            maskPaths[i] = assetsExtractPath + QString("/mask_512_%1.tiff").arg(chKeys[i]);

        // === Step 4: Per-channel FM screen, dot classification, promotion, packing.
        std::vector<std::vector<std::vector<uint8_t>>> allPacked(4); // [channel][row][byte]

        for (int ch = 0; ch < 4; ++ch) {
            
            // Load Image and Blue Noise Mask
            Magick::Image& channelImg = cmykChannels[ch];
            Magick::Image maskImg;
            maskImg.read(maskPaths[ch].toStdString());

            // Extract channel pixels (0..255 ink tone); "I" = intensity/gray.
			std::vector<uint8_t> channelBytes(width * height);
			channelImg.write(0, 0, width, height, "I", Magick::CharPixel, channelBytes.data());

			// Read the mask at its **native** size
			const int maskW = static_cast<int>(maskImg.columns());
			const int maskH = static_cast<int>(maskImg.rows());
			std::vector<uint8_t> maskRaw(maskW * maskH);
			maskImg.write(0, 0, maskW, maskH, "I", Magick::CharPixel, maskRaw.data());

			// FM screen with soft gate + highlight floor bias + scaled mask sampling
			std::vector<uint8_t> dithered(width * height, 0);	// Binary FM pass/fail (0/255).
			std::vector<uint8_t> classMask(width * height, 0); 	// Mask samples used for dot classification

			const bool isK = (ch == 3);
			const uint8_t floorRange = isK ? dotStrategy.floorRangeK : dotStrategy.floorRangeCMY;
			const uint8_t floorMax   = isK ? dotStrategy.floorMaxK   : dotStrategy.floorMaxCMY;
			const int minT  = std::clamp(dotStrategy.minInkThreshold, 0, 254);
			const int denom = 255 - minT;

			// Per-channel phase from seed; wrap in **mask** space (no scaling)
			const uint32_t h = pxhash(0xB, 0xC, ch, screenSeed);
			const int offXmask = static_cast<int>((h      ) & 0x7FFFFFFF) % maskW;
			const int offYmask = static_cast<int>((h >> 16) & 0x7FFFFFFF) % maskH;
			auto sampleMask = [&](int x, int y) -> uint8_t {
				int mx = x + offXmask; if (mx >= maskW) mx %= maskW;
				int my = y + offYmask; if (my >= maskH) my %= maskH;
				return maskRaw[my * maskW + mx];
			};

			// FM thresholding with highlight floor gating (prevents dropout in very light tones).
			for (int y = 0; y < height; ++y) {
				const int rowBase = y * width;
				for (int x = 0; x < width; ++x) {
					const int idx = rowBase + x;

					const uint8_t u = channelBytes[idx];
					if (u <= minT) { dithered[idx] = 0; classMask[idx] = 255; continue; }

					// Normalize [minT+1..255] → [0..255]
					uint16_t uEff = static_cast<uint16_t>(u - minT) * 255 / denom;

					// Gentle highlight floor (prevents dropouts in very light tones)
					if (floorRange > 0 && floorMax > 0 && uEff < floorRange) {
						const uint16_t bias = static_cast<uint16_t>(floorMax) * (floorRange - uEff) / floorRange;
						uEff = std::min<uint16_t>(255, uEff + bias);
					}

					const uint8_t t = sampleMask(x, y);
					dithered[idx]  = (uEff > t) ? 255 : 0;
					classMask[idx] = t; // Reuse the same mask value for dot-class decisions.
				}
			}

			// Dot classification uses mask-relative thresholds that adapt across tone.
			auto dotMap = dotClassification(dithered, classMask, channelBytes, width, height, dotStrategy);
            //auto dotMap = dotClassification(dithered, maskBytes, channelBytes, width, height, dotStrategy);

            // Optional neighborhood “promotion” to enlarge dots in dense regions (reduces peppering).
            if (dotStrategy.enablePromotion) {
                apply4x4Promotion(dotMap, channelBytes, width, height);
            }
			else {
				qDebug() << "Dot Promotion Disabled — skipping Dot Promotion.";
			}

            // Pack to 2bpp (4 pixels per byte, big-endian within the byte).
            auto packed = packTo2BPP(dotMap, width, height);
            allPacked[ch] = std::move(packed);
        }

        // === Step 5: Write PRN: Nocai header + interleaved channel rows in required order.
        return writePRNFile(allPacked, nocaiOrder, width, height, xdpi, ydpi, outputPath);

    } catch (const Magick::Exception& e) {
        qWarning() << "PRN generation failed:" << e.what();
        return false;
    }
}


// Attach input ICC (source) then destination ICC (printer), then force CMYK storage.
bool PrintJobNocai::applyICCConversion(const QString& inputProfile, const QString& outputProfile) {
    try {
		std::ifstream inFile(inputProfile.toStdString(), std::ios::binary);
		std::ifstream outFile(outputProfile.toStdString(), std::ios::binary);

        if (!inFile || !outFile) {
            qWarning() << "Failed to load one or both ICC profiles.";
            return false;
        }
        
        // Remove any embedded profiles before applying destination
        inputImage.profile("icc", Magick::Blob()); // Clear embedded profile

        // Load and apply input profile (e.g., sRGB)
        std::vector<char> inData((std::istreambuf_iterator<char>(inFile)), {});
        Magick::Blob inBlob(inData.data(), inData.size());
        inputImage.profile("icc", inBlob); // Attach source profile

        // Load and apply destination profile (e.g., CMYK printer)
        std::vector<char> outData((std::istreambuf_iterator<char>(outFile)), {});
        Magick::Blob outBlob(outData.data(), outData.size());
        inputImage.profile("icc", outBlob); // Apply new profile

        // Force Magick to convert to CMYK colorspace
		inputImage.colorSpace(Magick::CMYKColorspace);
		inputImage.type(Magick::ColorSeparationType);

        return true;

    } catch (const Magick::Exception& e) {
        qWarning() << "ICC conversion failed:" << e.what();
        return false;
    }
}


// Split CMYK to 4 grayscale planes (“I” format); 8-bit depth.
std::array<Magick::Image, 4> PrintJobNocai::separateCMYK(Magick::Image& cmykImage) {
    std::array<Magick::Image, 4> channels;
    int width = static_cast<int>(cmykImage.columns());
    int height = static_cast<int>(cmykImage.rows());

    std::vector<uchar> rawCMYK(width * height * 4);
    cmykImage.write(0, 0, width, height, "CMYK", Magick::CharPixel, rawCMYK.data());

    for (int ch = 0; ch < 4; ++ch) {
        std::vector<uchar> channelData(width * height);
        for (int i = 0; i < width * height; ++i)
            channelData[i] = rawCMYK[i * 4 + ch];

        channels[ch] = Magick::Image(Magick::Geometry(width, height), "white");
        channels[ch].depth(8);
        channels[ch].type(Magick::GrayscaleType);
        channels[ch].read(width, height, "I", Magick::CharPixel, channelData.data());
    }

    return channels;
}


// Set all ink dot thresholds and promotion toggle at once.
void PrintJobNocai::setDotStrategy(int minInkThreshold, int smallDotThreshold, int medDotThreshold, bool enablePromotion, uint8_t floorRangeCMY, uint8_t floorMaxCMY, uint8_t floorRangeK, uint8_t floorMaxK, bool enableDotSwap) {
    dotStrategy.minInkThreshold 	= minInkThreshold;
    dotStrategy.smallDotThreshold 	= smallDotThreshold;
    dotStrategy.medDotThreshold 	= medDotThreshold;
    dotStrategy.enablePromotion 	= enablePromotion;
    dotStrategy.floorRangeCMY 		= floorRangeCMY;
    dotStrategy.floorMaxCMY   		= floorMaxCMY;
    dotStrategy.floorRangeK   		= floorRangeK;
    dotStrategy.floorMaxK     		= floorMaxK;
    dotStrategy.enableDotSwap 		= enableDotSwap;
}


/* Dot classification
 * Input:
 *   dithered: result of FM screen (0/255)
 *   mask:     mask sample used for this pixel
 *   channel:  tone 0..255 (higher = more ink)
 *   thresholds: small/med cuts are “base” values adjusted by tone
 * Behavior:
 *   - Only pixels that passed dithered are classified.
 *   - tRel rescales the mask to 0..255 in the range that could pass at this tone.
 *   - small/med cuts are lerped against tone to keep highlight dots physically smaller.
 *   - Optional probabilistic swap small<->large in low tones to soften boundaries.
 */
std::vector<std::vector<uint8_t>> PrintJobNocai::dotClassification(
    const std::vector<uint8_t>& dithered,
    const std::vector<uint8_t>& mask,
    const std::vector<uint8_t>& channel,
    int width, int height,
    const DotStrategy& strategy)
{
    std::vector<std::vector<uint8_t>> dotMap(height, std::vector<uint8_t>(width, 0));

    // Base (ascending) cuts on the tone-normalized mask (tRel)
	const uint8_t smallBase = static_cast<uint8_t>(std::clamp(strategy.smallDotThreshold, 0, 255));
	const uint8_t medBase   = static_cast<uint8_t>(std::clamp(strategy.medDotThreshold,   0, 255));

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int idx = y * width + x;

            if (dithered[idx] == 0) continue;

            const uint8_t v = channel[idx]; // ink tone 0..255 (higher = more ink)
            const uint8_t t = mask[idx];    // mask 0..255
            
            // Normalize mask to the *passed* tone range: tRel ~ [0..255] regardless of v
            // When v is small, only low t values pass; this rescales those to full 0..255.
            const uint8_t tRel = uint8_t((uint16_t(t) * v) / 255);

            // Bias the cuts by tone so highlights tend toward larger dots (your original intent),
            // but without forcing *everything* to large at all tones.
            // At v=0 (very light): cuts are lower → relatively more medium/large than small.
            // At v=255 (dark): cuts revert to smallBase/medBase.
            const uint8_t smallCut = lerp_u8(64,  smallBase, v);  // 64 → smallBase across tone
            const uint8_t medCut   = lerp_u8(130, medBase,  v);   // 130 → medBase across tone

            if (tRel <= smallCut)           dotMap[y][x] = 1; // small
            else if (tRel <= medCut)        dotMap[y][x] = 2; // medium
            else                            dotMap[y][x] = 3; // large
            
			// Optional: swap small<->large in low tones (softly blended), medium stays medium
            if (strategy.enableDotSwap) {
                const uint8_t lo = 96;   // start of full-swap band
                const uint8_t hi = 160;  // end of probabilistic blend band

                if (v < lo) {
                    // Full swap (1 <-> 3)
                    dotMap[y][x] = static_cast<uint8_t>(4 - dotMap[y][x]);
                } else if (v < hi) {
                    // Probabilistic swap to avoid a hard contour
                    uint32_t h = pxhash(x, y, 0, 0x51F2F90Du);
                    uint8_t  p = static_cast<uint8_t>((uint16_t)(hi - v) * 255 / (hi - lo)); // 255..0
                    if ((h & 255) < p) {
                        dotMap[y][x] = static_cast<uint8_t>(4 - dotMap[y][x]);
                    }
                }
        	}
        }
    }
    return dotMap;
}


// Neighborhood promotion (4x4 window) to upsize dots in dense areas; avoids coarse grain in highlights.
void PrintJobNocai::apply4x4Promotion(std::vector<std::vector<uint8_t>>& dotMap,
                                      const std::vector<uint8_t>& tone,
                                      int width, int height) {
    // Promote only outside highlights to avoid coarse grain there
    const uint8_t promoteToneGate = 112;

    for (int y = 1; y < height - 2; ++y) {
        for (int x = 1; x < width - 2; ++x) {
            if (dotMap[y][x] == 3) continue;

            const int idx = y * width + x;
            if (tone[idx] < promoteToneGate) continue; // Skip promotion in light tones

            int count = 0;
            for (int dy = -1; dy <= 2; ++dy)
                for (int dx = -1; dx <= 2; ++dx)
                    if (dotMap[y + dy][x + dx] > 0)
                        ++count;

            if (count >= 12)
                dotMap[y][x] = 3;
        }
    }
}


// 2bpp packing: 4 pixels per byte, bits [7..0] = p0[1:0] p1[1:0] p2[1:0] p3[1:0]; pad lines to 4-byte boundary.
std::vector<std::vector<uint8_t>> PrintJobNocai::packTo2BPP(const std::vector<std::vector<uint8_t>>& dotMap, int width, int height) {
    const int bytesPerLine = (width + 3) / 4;
    std::vector<std::vector<uint8_t>> packedLines(height);

    for (int y = 0; y < height; ++y) {
        std::vector<uint8_t>& line = packedLines[y];
        line.reserve(bytesPerLine);
        uint8_t byte = 0;
        int idx = 0;

        for (int x = 0; x < width; ++x) {
            uint8_t level = dotMap[y][x] & 0x03;
            byte |= level << ((3 - (idx % 4)) * 2);
            ++idx;

            if (idx % 4 == 0) {
                line.push_back(byte);
                byte = 0;
            }
        }

        if (idx % 4 != 0) line.push_back(byte);
        while (line.size() % 4 != 0) line.push_back(0);
    }

    return packedLines;
}


/* PRN writer
 * Header (48 bytes, 12 x uint32_t):
 *   [0]  signature 0x00005555
 *   [1]  xdpi
 *   [2]  ydpi
 *   [3]  bytesPerLine (per channel)
 *   [4]  height
 *   [5]  width
 *   [6]  paperWidth (0 = unspecified)
 *   [7]  colors (4)
 *   [8]  bits   (1 => 2bpp levels encoded externally)
 *   [9]  pass   (1)
 *   [10] vsdMode (0)
 *   [11] reserved (0)
 * Data: for each row, write channels in channelOrder, each as packedLines[ch][row].
 */
bool PrintJobNocai::writePRNFile(
        const std::vector<std::vector<std::vector<uint8_t>>>& packedLines,
        const std::vector<int>& channelOrder,
        int width, int height, int xdpi, int ydpi,
        const QString& outputPath)
    {

    QString outPath = QUrl(outputPath).toLocalFile();

    std::ofstream out(outPath.toStdString(), std::ios::binary);
    if (!out) {
        qWarning() << "Failed to open output file for writing:" << outputPath;
        return false;
    }

    uint32_t bytesPerLine = static_cast<uint32_t>(packedLines[0][0].size());

    uint32_t header[12] = {
        0x00005555,						// Signature
        static_cast<uint32_t>(xdpi),	// XDPI
        static_cast<uint32_t>(ydpi),	// YDPI
        bytesPerLine,					// BytesPerLine
        static_cast<uint32_t>(height),	// Height
        static_cast<uint32_t>(width),	// Width
        0,								// PaperWidth
        4,								// Colors
        1,								// Bits
        1, 								// Pass
        0,								// VSD Mode
        0								// Reserved[0]
    };
    
    out.write(reinterpret_cast<const char*>(header), sizeof(header));

    for (int row = 0; row < height; ++row) {
        for (int ch : channelOrder) {
            out.write(reinterpret_cast<const char*>(packedLines[ch][row].data()), packedLines[ch][row].size());
        }
    }

    out.close();
    qDebug() << "Final PRN file created:" << outputPath;
    return true;
}


// Read image and stage a temp file to work from (avoids touching originals).
bool PrintJobNocai::loadInputImage(const QString& imagePath) {
    try {
        QString localPath = QUrl(imagePath).toLocalFile();
        inputImage.read(localPath.toStdString());

        QFileInfo fileInfo(localPath);
        originalFilename = fileInfo.fileName();

        tempDir = std::make_unique<QTemporaryDir>();
        if (!tempDir->isValid()) {
            qWarning() << "Failed to create temp dir";
            return false;
        }

        tempImagePath = tempDir->filePath(originalFilename);
        inputImage.write(tempImagePath.toStdString());

        qDebug() << "Loaded and copied input image to:" << tempImagePath;
        return true;
    } catch (const Magick::Exception& e) {
        qWarning() << "Image load failed:" << e.what();
        return false;
    }
}


// Read an ICC file into a Magick::Blob.
Magick::Blob PrintJobNocai::loadICCProfile(const QString& path) {
    std::ifstream file(path.toStdString(), std::ios::binary);
    std::vector<char> buf((std::istreambuf_iterator<char>(file)), {});
    return Magick::Blob(buf.data(), buf.size());
}


// Default output ICC path setter and getter
void PrintJobNocai::setDefaultOutputICCProfile(const QString& outputProfile) {
    defaultOutputICCPath = outputProfile;
    qDebug() << "Default output ICC profile set to:" << outputProfile;
}

QString PrintJobNocai::getDefaultOutputICCProfile() const { return defaultOutputICCPath; }


// Default input CMYK path setter and getter
void PrintJobNocai::setDefaultInputCMYKProfile(const QString& inputProfilePath) {
    defaultInputCMYKPath = inputProfilePath;
    qDebug() << "Default input CMYK profile set to:" << inputProfilePath;
}

QString PrintJobNocai::getDefaultInputCMYKProfile() const { return defaultInputCMYKPath; }


// Global toggle for CMYK->printer conversion (when source is already CMYK).
void PrintJobNocai::enableDefaultInputCMYK(bool enabled) {
    useDefaultInputCMYK = enabled;
    qDebug() << "Use default CMYK input profile:" << enabled;
}

bool PrintJobNocai::checkDefaultInputCMYK() const { return useDefaultInputCMYK; }


// Add a named ICC profile to the in-memory list.
void PrintJobNocai::addICCProfile(const QString& name, const QString& path) {
    availableICCProfiles.append({name, path});
}


// Return available ICC profiles as [{name,path}, ...].
QVariantList PrintJobNocai::getAvailableICCProfiles() const {
    QVariantList list;
    for (const auto& pair : availableICCProfiles) {
        QVariantMap entry;
        entry["name"] = pair.first;
        entry["path"] = pair.second;
        list.append(entry);
    }
    qDebug() << "Returning" << list.size() << "available ICC profiles.";
    return list;
}


// Copy runtime assets (profiles and masks) out of resources to a writeable temp location.
// Also initialize defaults and register profiles for UI selection.
void PrintJobNocai::prepareNocaiAssets() {
    assetsExtractPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/runtime_assets";
    QDir().mkpath(assetsExtractPath);

    auto copyIfMissing = [](const QString& qrcPath, const QString& destPath) {
        if (!QFile::exists(destPath)) {
            QFile::copy(qrcPath, destPath);
        }
    };

    // ICC Profiles
    copyIfMissing(":/assets/sRGBProfile.icm", assetsExtractPath + "/sRGBProfile.icm");
    copyIfMissing(":/assets/RIP_App_Plain_720.icm", assetsExtractPath + "/RIP_App_Plain_720.icm");
    copyIfMissing(":/assets/RIP_App_Generic_CMYK.icc", assetsExtractPath + "/RIP_App_Generic_CMYK.icc");

    // Blue Noise Masks (256x256 Tiles, 12000x12000 Size)
    copyIfMissing(":/assets/blue_noise_mask_256_12000/mask_c.tiff", assetsExtractPath + "/mask_256_c.tiff");
    copyIfMissing(":/assets/blue_noise_mask_256_12000/mask_m.tiff", assetsExtractPath + "/mask_256_m.tiff");
    copyIfMissing(":/assets/blue_noise_mask_256_12000/mask_y.tiff", assetsExtractPath + "/mask_256_y.tiff");
    copyIfMissing(":/assets/blue_noise_mask_256_12000/mask_k.tiff", assetsExtractPath + "/mask_256_k.tiff");
    
    // Blue Noise Masks (512x512 Tiles, 12000x12000 Size)
    copyIfMissing(":/assets/blue_noise_mask_512_12000/mask_c.tiff", assetsExtractPath + "/mask_512_c.tiff");
    copyIfMissing(":/assets/blue_noise_mask_512_12000/mask_m.tiff", assetsExtractPath + "/mask_512_m.tiff");
    copyIfMissing(":/assets/blue_noise_mask_512_12000/mask_y.tiff", assetsExtractPath + "/mask_512_y.tiff");
    copyIfMissing(":/assets/blue_noise_mask_512_12000/mask_k.tiff", assetsExtractPath + "/mask_512_k.tiff");
    
	auto addProfile = [&](const QString& name, const QString& qrcPath, const QString& fileName) {
		QString destPath = assetsExtractPath + "/" + fileName;
		if (!QFile::exists(destPath)) {
			QFile::copy(qrcPath, destPath);
		}
		addICCProfile(name, destPath);
	};

    // Register ICC Profiles for UI and set defaults.
    addProfile("Plain Paper (720DPI)",  ":/assets/RIP_App_Plain_720.icm", "RIP_App_Plain_720.icm");
    addProfile("sRGB Input", ":/assets/sRGBProfile.icm", "sRGBProfile.icm");
    addProfile("CMYK Input", ":/assets/RIP_App_Generic_CMYK.icc", "RIP_App_Generic_CMYK.icc");
    setDefaultOutputICCProfile((assetsExtractPath + "/RIP_App_Plain_720.icm"));
    setDefaultInputCMYKProfile(assetsExtractPath + "/RIP_App_Generic_CMYK.icc");

    qDebug() << "Nocai assets prepared in:" << assetsExtractPath;
}


// Remove intermediate files for a given job (and optionally its working directory).
void PrintJobNocai::cleanupTemporaryFiles(const QString& baseName, const QString& workingDir) {
    qDebug() << "Cleaning intermediate files for base:" << baseName << "in dir:" << workingDir;
    QStringList suffixes = {
        "_c_1bit.tiff", "_m_1bit.tiff", "_y_1bit.tiff", "_k_1bit.tiff",
        "_c.tiff", "_m.tiff", "_y.tiff", "_k.tiff",
        "_cmyk.tiff",
        "_c_mask.tiff", "_m_mask.tiff", "_y_mask.tiff", "_k_mask.tiff"
    };

    for (const QString& suffix : suffixes) {
        QString path = workingDir + "/" + baseName + suffix;
        if (QFile::exists(path)) {
            QFile::remove(path);
        }
    }
    
    // Optionally remove entire working dir
    QDir dir(workingDir);
    if (dir.exists()) {
        dir.removeRecursively();
        qDebug() << "Working directory removed:" << workingDir;
    }
}


// Remove all runtime assets from the app data location.
void PrintJobNocai::cleanupRuntimeAssets() {
    qDebug() << "Cleaning runtime assets in:" << assetsExtractPath;
    QDir dir(assetsExtractPath);
    if (dir.exists()) {
        dir.removeRecursively();
        qDebug() << "Runtime assets cleaned.";
    } else {
        qDebug() << "Runtime assets directory not found.";
    }
}


// Commented out a simple Dot Strategy without gating or tonal awareness. Much faster and works for 90% of files

/*
std::vector<std::vector<uint8_t>> PrintJobNocai::dotClassification(const std::vector<uint8_t>& dithered, const std::vector<uint8_t>& mask, const std::vector<uint8_t>& channel, int width, int height, const DotStrategy& strategy) {
	std::vector<std::vector<uint8_t>> dotMap(height, std::vector<uint8_t>(width, 0));
	
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (dithered[y * width + x] < 128) continue;

            uint8_t t = mask[y * width + x];
            
			if (t >= strategy.smallDotThreshold) {
                dotMap[y][x] = 1; // Small dot
            } else if (t >= strategy.medDotThreshold) {
                dotMap[y][x] = 2; // Medium dot
            } else {
                dotMap[y][x] = 3; // Large dot
            }
        }
    }
    return dotMap;
}


void PrintJobNocai::apply4x4Promotion(std::vector<std::vector<uint8_t>>& dotMap, int width, int height) {
    
    for (int y = 1; y < height - 2; ++y) {
        for (int x = 1; x < width - 2; ++x) {
            if (dotMap[y][x] == 3) continue;

            int count = 0;
            for (int dy = -1; dy <= 2; ++dy)
                for (int dx = -1; dx <= 2; ++dx)
                    if (dotMap[y + dy][x + dx] > 0)
                        ++count;

            if (count >= 12)
                dotMap[y][x] = 3;
        }
    }
}
*/
