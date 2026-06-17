#include "PrintJobCMYK.h"
#include "NocaiPrnWriter.h"
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QProcess>
#include <QDebug>
#include <QUrl>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>


/* PrintJobCMYK.cpp
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
PrintJobCMYK::PrintJobCMYK(QObject* parent) : QObject(parent) {}


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
void PrintJobCMYK::runPRNGeneration(const QVariantMap& jobMap, const QString& outputPath) {
    (void) QtConcurrent::run([=]() {
        bool success = false;

        // Ensure runtime assets are ready
        const_cast<PrintJobCMYK*>(this)->prepareAssets();
        qDebug() << "PrintJobCMYK::runPRNGeneration: assetsExtractPath =" << assetsExtractPath;

        // ---- Basic job fields ----
        const QString imagePath  = jobMap.value("imagePath").toString();
        const QSize   resolution = jobMap.value("resolution").toSize();

        // ============================================================
        // Dot Strategy: ColorManager ALWAYS takes precedence
        // (No per-job dot strategy overrides; jobMap keys are ignored)
        // ============================================================
        const int  minThreshold     = (m_colorManager ? m_colorManager->minInkThreshold()   : dotStrategy.minInkThreshold);
        const int  smallThreshold   = (m_colorManager ? m_colorManager->smallDotThreshold() : dotStrategy.smallDotThreshold);
        const int  medThreshold     = (m_colorManager ? m_colorManager->medDotThreshold()   : dotStrategy.medDotThreshold);
        const bool promotionEnabled = (m_colorManager ? m_colorManager->enablePromotion()   : dotStrategy.enablePromotion);

        const int  floorRangeCMY    = (m_colorManager ? m_colorManager->floorRangeCMY()     : int(dotStrategy.floorRangeCMY));
        const int  floorMaxCMY      = (m_colorManager ? m_colorManager->floorMaxCMY()       : int(dotStrategy.floorMaxCMY));
        const int  floorRangeK      = (m_colorManager ? m_colorManager->floorRangeK()       : int(dotStrategy.floorRangeK));
        const int  floorMaxK        = (m_colorManager ? m_colorManager->floorMaxK()         : int(dotStrategy.floorMaxK));
        const bool enableDotSwap    = (m_colorManager ? m_colorManager->enableDotSwap()     : dotStrategy.enableDotSwap);

        const_cast<PrintJobCMYK*>(this)->setDotStrategy(
            minThreshold,
            smallThreshold,
            medThreshold,
            promotionEnabled,
            static_cast<uint8_t>(std::clamp(floorRangeCMY, 0, 64)),
            static_cast<uint8_t>(std::clamp(floorMaxCMY,   0,  8)),
            static_cast<uint8_t>(std::clamp(floorRangeK,   0, 64)),
            static_cast<uint8_t>(std::clamp(floorMaxK,     0,  8)),
            enableDotSwap
        );

        // ---- Resolve output ICC (job override if it's a REAL ICC file -> else ColorManager -> else default asset) ----
        auto normalizeLocalPath = [](const QString& s) -> QString {
            if (s.startsWith("file:", Qt::CaseInsensitive))
                return QUrl(s).toLocalFile();
            return s;
        };

        auto looksLikeIccPath = [&](const QString& s) -> bool {
            const QString p = normalizeLocalPath(s).trimmed();
            if (p.isEmpty()) return false;
            const QString low = p.toLower();
            if (!(low.endsWith(".icc") || low.endsWith(".icm"))) return false;
            return QFileInfo::exists(p);
        };

        const QString jobColorProfile = jobMap.value("colorProfile").toString().trimmed();

        QString outputICC;
        if (looksLikeIccPath(jobColorProfile)) {
            outputICC = normalizeLocalPath(jobColorProfile);
        } else if (m_colorManager) {
            const QString cmOut = m_colorManager->effectiveOutputProfile().trimmed();
            if (looksLikeIccPath(cmOut))
                outputICC = normalizeLocalPath(cmOut);
        }
        if (outputICC.isEmpty()) {
            outputICC = defaultOutputICCPath; // internal default in assets dir
        }

        qDebug() << "PrintJobCMYK: dot strategy updated (ColorManager precedence)"
                 << "minInk=" << dotStrategy.minInkThreshold
                 << "small="  << dotStrategy.smallDotThreshold
                 << "med="    << dotStrategy.medDotThreshold
                 << "promo="  << dotStrategy.enablePromotion
                 << "dotSwap="<< dotStrategy.enableDotSwap
                 << "floor(CMY)=" << dotStrategy.floorRangeCMY << "/" << dotStrategy.floorMaxCMY
                 << "floor(K)="   << dotStrategy.floorRangeK   << "/" << dotStrategy.floorMaxK
                 << "outputICC="  << outputICC;

        // ---- Load image ----
        if (loadInputImage(imagePath)) {

            // ---- ICC conversion logic ----
            if (inputImage.colorSpace() != Magick::CMYKColorspace) {
                qDebug() << "PrintJobCMYK: input NOT CMYK — applying ICC (sRGB → printer CMYK)";

                const QString inputICC = assetsExtractPath + "/sRGBProfile.icm";
                if (!outputICC.isEmpty()) {
                    success = applyICCConversion(inputICC, outputICC);
                } else {
                    qWarning() << "PrintJobCMYK: no output ICC available; cannot convert.";
                    success = false;
                }

            } else {
                if (useDefaultInputCMYK) {
                    // Prefer ColorManager default input CMYK if set; otherwise fall back to internal defaultInputCMYKPath
                    QString inCMYK = defaultInputCMYKPath;
                    if (m_colorManager) {
                        const QString cmIn = m_colorManager->defaultInputProfile().trimmed();
                        if (!cmIn.isEmpty())
                            inCMYK = cmIn;
                    }

                    if (!inCMYK.isEmpty() && !outputICC.isEmpty()) {
                        qDebug() << "PrintJobCMYK: input CMYK — applying ICC (Default CMYK → printer CMYK)";
                        success = applyICCConversion(inCMYK, outputICC);
                    } else {
                        qWarning() << "PrintJobCMYK: CMYK input ICC or output ICC missing; skipping conversion.";
                        success = true; // keep pipeline running
                    }

                } else {
                    qDebug() << "PrintJobCMYK: input CMYK — skipping ICC conversion.";
                    success = true;
                }
            }

            // Seed FM phase
            screenSeed = qHash(imagePath) ^ static_cast<uint32_t>(QDateTime::currentMSecsSinceEpoch() & 0xFFFFFFFF);

            // Generate PRN
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
bool PrintJobCMYK::generateFinalPRN(const QString& outputPath, int xdpi, int ydpi) {
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
            auto packed = NocaiPrnWriter::packTo2Bpp(dotMap, width, height);
            allPacked[ch] = std::move(packed);
        }

        return NocaiPrnWriter::writeStandardCmykPrn(
            allPacked,
            nocaiOrder,
            width,
            height,
            xdpi,
            ydpi,
            outputPath);

    } catch (const Magick::Exception& e) {
        qWarning() << "PRN generation failed:" << e.what();
        return false;
    }
}


// Attach input ICC (source) then destination ICC (printer), then force CMYK storage.
bool PrintJobCMYK::applyICCConversion(const QString& inputProfile, const QString& outputProfile) {
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
std::array<Magick::Image, 4> PrintJobCMYK::separateCMYK(Magick::Image& cmykImage) {
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

void PrintJobCMYK::setColorManager(ColorManagementManager* mgr) {
    m_colorManager = mgr;
}



// Set all ink dot thresholds and promotion toggle at once.
void PrintJobCMYK::setDotStrategy(int minInkThreshold, int smallDotThreshold, int medDotThreshold, bool enablePromotion, uint8_t floorRangeCMY, uint8_t floorMaxCMY, uint8_t floorRangeK, uint8_t floorMaxK, bool enableDotSwap) {
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
std::vector<std::vector<uint8_t>> PrintJobCMYK::dotClassification(
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





// Gradual, one-step-only promotion with soft probability bands.
// - Small -> Medium in a lower band
// - Medium -> Large in a higher band
// - No Small -> Large jump
// - Tone-gated; avoids highlights and edges
void PrintJobCMYK::apply4x4Promotion(std::vector<std::vector<uint8_t>>& dotMap,
                                      const std::vector<uint8_t>& tone,
                                      int width, int height)
{
    // ---- Tunables (gentle defaults) ----
    const uint8_t  TONE_GATE        = 112;  // don’t promote in highlights
    const int      MED_LO           = 18;   // weighted sum lower bound to *start* small->med
    const int      MED_HI           = 26;   // upper bound where small->med becomes likely
    const int      LRG_LO           = 28;   // weighted sum lower bound to *start* med->large
    const int      LRG_HI           = 36;   // upper bound where med->large becomes likely
    const int      FLAT_VAR_EPS     = 18;   // require fairly flat local tone (lower = stricter)
    const int      MIN_NEI_INKED    = 8;    // need at least N/15 neighbors inked before considering
    const int      KICK_BONUS       = 2;    // small, gentle bias (instead of big bias/override)

    auto localMAD = [&](int cx, int cy) -> int {
        int sum = 0, cnt = 0;
        const int idxC = cy * width + cx;
        const int vC = tone[idxC];
        for (int dy = -1; dy <= 2; ++dy) {
            int y = cy + dy; if (y < 0 || y >= height) continue;
            for (int dx = -1; dx <= 2; ++dx) {
                int x = cx + dx; if (x < 0 || x >= width) continue;
                if (dx == 0 && dy == 0) continue;
                sum += std::abs(int(tone[y*width + x]) - vC);
                ++cnt;
            }
        }
        return cnt ? (sum / cnt) : 0;
    };

    auto lerp01 = [](int v, int lo, int hi) -> float {
        if (v <= lo) return 0.f;
        if (v >= hi) return 1.f;
        return float(v - lo) / float(hi - lo);
    };

    for (int y = 1; y < height - 2; ++y) {
        for (int x = 1; x < width - 2; ++x) {

            uint8_t cls = dotMap[y][x];          // 0..3
            if (cls == 0 || cls == 3) continue;  // only consider small(1) or medium(2)
            const int idx = y * width + x;
            const uint8_t v = tone[idx];
            if (v < TONE_GATE) continue;         // skip highlights

            // Neighborhood stats (4×4, no center)
            int weighted = 0;
            int countAny = 0;
            for (int dy = -1; dy <= 2; ++dy) {
                for (int dx = -1; dx <= 2; ++dx) {
                    if (dx == 0 && dy == 0) continue;
                    uint8_t ncls = dotMap[y + dy][x + dx]; // 0..3
                    if (ncls > 0) ++countAny;
                    weighted += int(ncls);
                }
            }

            if (countAny < MIN_NEI_INKED) continue;  // too sparse → no promotion
            if (localMAD(x, y) > FLAT_VAR_EPS) continue; // edges/textures → no promotion

            // Small gentle bias (replaces big bonuses/overrides)
            weighted += KICK_BONUS;

            // Tone factor (0..1): higher tone should be more willing to coarsen
            const float toneF = float(v) / 255.0f;

            // Deterministic RNG from coords (no flicker between runs)
            const uint32_t rnd = pxhash(x, y, 0, 0x51F2F90Du) & 0xFFFF;
            const float r01 = float(rnd) / 65535.0f;

            // Decide the band and compute probability
            if (cls == 1) {
                // small -> medium
                float p = lerp01(weighted, MED_LO, MED_HI) * toneF; // soften by tone
                if (r01 < p) dotMap[y][x] = 2;
            } else if (cls == 2) {
                // medium -> large
                float p = lerp01(weighted, LRG_LO, LRG_HI) * toneF;
                if (r01 < p) dotMap[y][x] = 3;
            }
        }
    }
}





// Read image and stage a temp file to work from (avoids touching originals).
bool PrintJobCMYK::loadInputImage(const QString& imagePath) {
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
Magick::Blob PrintJobCMYK::loadICCProfile(const QString& path) {
    std::ifstream file(path.toStdString(), std::ios::binary);
    std::vector<char> buf((std::istreambuf_iterator<char>(file)), {});
    return Magick::Blob(buf.data(), buf.size());
}


// Default output ICC path setter and getter
void PrintJobCMYK::setDefaultOutputICCProfile(const QString& outputProfile) {
    defaultOutputICCPath = outputProfile;
    qDebug() << "Default output ICC profile set to:" << outputProfile;
}

QString PrintJobCMYK::getDefaultOutputICCProfile() const { return defaultOutputICCPath; }


// Default input CMYK path setter and getter
void PrintJobCMYK::setDefaultInputCMYKProfile(const QString& inputProfilePath) {
    defaultInputCMYKPath = inputProfilePath;
    qDebug() << "Default input CMYK profile set to:" << inputProfilePath;
}

QString PrintJobCMYK::getDefaultInputCMYKProfile() const { return defaultInputCMYKPath; }


// Global toggle for CMYK->printer conversion (when source is already CMYK).
void PrintJobCMYK::enableDefaultInputCMYK(bool enabled) {
    useDefaultInputCMYK = enabled;
    qDebug() << "Use default CMYK input profile:" << enabled;
}

bool PrintJobCMYK::checkDefaultInputCMYK() const { return useDefaultInputCMYK; }


// Add a named ICC profile to the in-memory list.
void PrintJobCMYK::addICCProfile(const QString& name, const QString& path) {
    // Avoid duplicates: if this path is already in the list, skip adding.
    for (const auto& pair : availableICCProfiles) {
        if (pair.second == path) {
            qDebug() << "PrintJobCMYK::addICCProfile: skipping duplicate ICC profile at"
                     << path;
            return;
        }
    }
    
    availableICCProfiles.append({name, path});
}


// Return available ICC profiles as [{name,path}, ...].
QVariantList PrintJobCMYK::getAvailableICCProfiles() const {
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
void PrintJobCMYK::prepareAssets() {
    if (!m_assetManager.initialize("runtime_assets")) {
        qWarning() << "PrintJobCMYK: failed to initialize AssetManager.";
        return;
    }

    assetsExtractPath = m_assetManager.rootPath();

    // If assets have been moved and directory still exists, skip all the work.
    if (assetsPrepared && !assetsExtractPath.isEmpty() && QDir(assetsExtractPath).exists()) {
        qDebug() << "PrintJobCMYK: assets already prepared in" << assetsExtractPath;
        return;
    }

    const QStringList bundledResourcePaths = {
        ":/assets/sRGBProfile.icm",
        ":/assets/RIP_App_1440_Plain_Default.icc",
        ":/assets/RIP_App_1440_Plain_Neutral.icc",
        ":/assets/RIP_App_Generic_CMYK.icc"
    };

    const QStringList bundledFileNames = {
        "sRGBProfile.icm",
        "RIP_App_1440_Plain_Default.icc",
        "RIP_App_1440_Plain_Neutral.icc",
        "RIP_App_Generic_CMYK.icc"
    };

    if (!m_assetManager.copyResourcesIfMissing(bundledResourcePaths, bundledFileNames)) {
        qWarning() << "PrintJobCMYK: failed to copy one or more bundled runtime assets.";
        return;
    }

    const QStringList maskKeys = {"c", "m", "y", "k"};
    for (const QString& key : maskKeys) {
        const QString resourcePath = QString(":/assets/blue_noise_mask_512_12000/mask_%1.tiff").arg(key);
        const QString fileName = QString("mask_512_%1.tiff").arg(key);
        if (m_assetManager.hasAsset(fileName))
            continue;
        if (QFile::exists(resourcePath)) {
            (void)m_assetManager.copyResourceIfMissing(resourcePath, fileName);
        } else {
            qWarning() << "PrintJobCMYK: mask is not bundled and is missing from runtime assets:" << fileName;
        }
    }
    
	auto addProfile = [&](const QString& name, const QString& qrcPath, const QString& fileName) {
        if (!m_assetManager.copyResourceIfMissing(qrcPath, fileName)) {
            qWarning() << "PrintJobCMYK: failed to copy ICC profile:" << qrcPath;
            return;
        }
        addICCProfile(name, m_assetManager.assetPath(fileName));
	};

    // Register ICC Profiles for UI and set defaults.
    addProfile("Default - Plain Paper (1440DPI)",  ":/assets/RIP_App_1440_Plain_Default.icc", "RIP_App_1440_Plain_Default.icc");
    addProfile("Neutral Profile - Plain Paper (1440DPI)",  ":/assets/RIP_App_1440_Plain_Neutral.icc", "RIP_App_1440_Plain_Neutral.icc");
    addProfile("sRGB Input", ":/assets/sRGBProfile.icm", "sRGBProfile.icm");
    addProfile("CMYK Input", ":/assets/RIP_App_Generic_CMYK.icc", "RIP_App_Generic_CMYK.icc");
    setDefaultOutputICCProfile(m_assetManager.assetPath("RIP_App_1440_Plain_Default.icc"));
    setDefaultInputCMYKProfile(m_assetManager.assetPath("RIP_App_Generic_CMYK.icc"));

    assetsPrepared = true;

    qDebug() << "PrintJobCMYK assets prepared in:" << assetsExtractPath;
}


// Remove intermediate files for a given job (and optionally its working directory).
void PrintJobCMYK::cleanupTemporaryFiles(const QString& baseName, const QString& workingDir) {
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
void PrintJobCMYK::cleanupRuntimeAssets() {
    qDebug() << "Cleaning runtime assets in:" << assetsExtractPath;
    if (m_assetManager.cleanup()) {
        assetsPrepared = false;
        qDebug() << "Runtime assets cleaned.";
    } else {
        qWarning() << "Failed to clean runtime assets.";
    }
}


// Commented out a simple Dot Strategy without gating or tonal awareness. Much faster and works for 90% of files

/*
std::vector<std::vector<uint8_t>> PrintJobCMYK::dotClassification(const std::vector<uint8_t>& dithered, const std::vector<uint8_t>& mask, const std::vector<uint8_t>& channel, int width, int height, const DotStrategy& strategy) {
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


void PrintJobCMYK::apply4x4Promotion(std::vector<std::vector<uint8_t>>& dotMap, int width, int height) {
    
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
