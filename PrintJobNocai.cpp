#include "PrintJobNocai.h"
#include <lcms2.h>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QFileInfo>
#include <QDirIterator>
#include <QProcess>
#include <QDebug>
#include <QUrl>
#include <fstream>


// Constructor
PrintJobNocai::PrintJobNocai(QObject* parent) : QObject(parent) {}


// Load image and copy to a temporary location
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


Magick::Blob PrintJobNocai::loadICCProfile(const QString& path) {
    std::ifstream file(path.toStdString(), std::ios::binary);
    std::vector<char> buf((std::istreambuf_iterator<char>(file)), {});
    return Magick::Blob(buf.data(), buf.size());
}


/* // Little CMS ICC Conversion
bool PrintJobNocai::applyICCConversion(const QString& inputProfile, const QString& outputProfile) {
    try {
	QString inPath = inputProfile;
	QString outPath = outputProfile;

        // Load ICC profiles
        cmsHPROFILE inputICC = cmsOpenProfileFromFile(inPath.toStdString().c_str(), "r");
        cmsHPROFILE outputICC = cmsOpenProfileFromFile(outPath.toStdString().c_str(), "r");
        if (!inputICC || !outputICC) {
            qWarning() << "Failed to load one or both ICC profiles.";
            if (inputICC) cmsCloseProfile(inputICC);
            if (outputICC) cmsCloseProfile(outputICC);
            return false;
        }

        // Create Little CMS transform
        cmsHTRANSFORM transform = cmsCreateTransform(inputICC, TYPE_RGB_8,
                                                     outputICC, TYPE_CMYK_8,
                                                     INTENT_PERCEPTUAL, 0);
        if (!transform) {
            qWarning() << "Failed to create ICC transform.";
            cmsCloseProfile(inputICC);
            cmsCloseProfile(outputICC);
            return false;
        }

        // Prepare input and output buffers
        int width = static_cast<int>(inputImage.columns());
        int height = static_cast<int>(inputImage.rows());
        inputImage.depth(8);  // Ensures 8-bit component depth
        std::vector<uchar> rgbBuffer(width * height * 3);
        std::vector<uchar> cmykBuffer(width * height * 4);

        inputImage.type(Magick::TrueColorType);
        inputImage.colorSpace(Magick::RGBColorspace);
        inputImage.write(0, 0, width, height, "RGB", Magick::CharPixel, rgbBuffer.data());

        // Apply color conversion
        cmsDoTransform(transform, rgbBuffer.data(), cmykBuffer.data(), width * height);

        cmsDeleteTransform(transform);
        cmsCloseProfile(inputICC);
        cmsCloseProfile(outputICC);

	// Create CMYK Magick image from raw buffer (no constructor)
	cmykImage = Magick::Image(Magick::Geometry(width, height), "white");
	cmykImage.depth(8);

	// Set correct color space and pixel interpretation *before* reading in data
	cmykImage.modifyImage();
	cmykImage.colorSpace(Magick::CMYKColorspace);
	cmykImage.type(Magick::ColorSeparationType);

	// Now load pixels into image
	cmykImage.read(width, height, "CMYK", Magick::CharPixel, cmykBuffer.data());

	// Force set again in case it got internally overridden
	cmykImage.colorSpace(Magick::CMYKColorspace);
	cmykImage.type(Magick::ColorSeparationType);

	qDebug() << "cmykImage colorspace enum:" << static_cast<int>(cmykImage.colorSpace());


        // Embed the output ICC profile into CMYK image
        std::ifstream profileFile(outPath.toStdString(), std::ios::binary);
        if (profileFile) {
            std::vector<char> profileData((std::istreambuf_iterator<char>(profileFile)), {});
            Magick::Blob profileBlob(profileData.data(), profileData.size());
            cmykImage.profile("icc", profileBlob);
        }

        qDebug() << "ICC conversion succeeded using:" << inPath << "→" << outPath;
        return true;

    } catch (const Magick::Exception& e) {
        qWarning() << "Exception during ICC conversion:" << e.what();
        return false;
    }
}
*/


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


bool PrintJobNocai::generateFinalPRN(const QString& outputPath, int xdpi, int ydpi) {
    try {
        const std::vector<int> nocaiOrder = {2, 1, 0, 3}; // Y, M, C, K
        const QStringList chKeys = {"c", "m", "y", "k"};

        if (inputImage.colorSpace() != Magick::CMYKColorspace) {
            qWarning() << "Input image is not in CMYK colorspace.";
            return false;
        }
        
        // === Step 1: Apply scaling based on DPI        
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
        
        // === Step 2: Separate CMYK channels
		auto cmykChannels = separateCMYK(inputImage);
        int width = static_cast<int>(cmykChannels[0].columns());
        int height = static_cast<int>(cmykChannels[0].rows());

        // === Step 3: Prepare blue noise mask paths
        std::array<QString, 4> maskPaths;
        for (int i = 0; i < 4; ++i)
            maskPaths[i] = assetsExtractPath + QString("/mask_256_%1.tiff").arg(chKeys[i]);

        // === Step 4: Process each channel
        std::vector<std::vector<std::vector<uint8_t>>> allPacked(4); // [channel][row][byte]

        for (int ch = 0; ch < 4; ++ch) {
            Magick::Image& channelImg = cmykChannels[ch];
            Magick::Image maskImg;
            maskImg.read(maskPaths[ch].toStdString());

            // Extract raw pixel data
            std::vector<uint8_t> channelBytes(width * height);
            std::vector<uint8_t> maskBytes(width * height);
            channelImg.write(0, 0, width, height, "I", Magick::CharPixel, channelBytes.data());
            maskImg.write(0, 0, width, height, "I", Magick::CharPixel, maskBytes.data());
            
            // Apply FM screen (u >= v ? 1 : 0)
            std::vector<uint8_t> dithered(width * height, 0);
			const uint8_t INK_THRESHOLD = 2;  // Minimum value for ink to be considered

			for (int i = 0; i < width * height; ++i) {
				if (channelBytes[i] < INK_THRESHOLD) {
					dithered[i] = 0;  // Treat as blank — do not screen
				} else {
					dithered[i] = (channelBytes[i] >= maskBytes[i]) ? 255 : 0;
				}
			}


/*            std::vector<uint8_t> dithered(width * height, 0);
            for (int i = 0; i < width * height; ++i)
                dithered[i] = (channelBytes[i] >= maskBytes[i]) ? 255 : 0;
*/


            // Classify dots by mask thresholds
            auto dotMap = dotClassification(dithered, maskBytes, width, height);

            // Promote small/medium dots
            apply4x4Promotion(dotMap, width, height);

            // Pack into 2BPP format
            auto packed = packTo2BPP(dotMap, width, height);
            allPacked[ch] = std::move(packed);
        }

        // === Step 5: Write PRN file
        return writePRNFile(allPacked, nocaiOrder, width, height, xdpi, ydpi, outputPath);

    } catch (const Magick::Exception& e) {
        qWarning() << "PRN generation failed:" << e.what();
        return false;
    }
}



// === Helper Functions ===

void PrintJobNocai::runPRNGeneration(const QVariantMap& jobMap, const QString& outputPath) {
    (void) QtConcurrent::run([=]() {
        bool success = false;
        
        const QString imagePath = jobMap["imagePath"].toString();
        const QSize resolution = jobMap["resolution"].toSize();
        const QString colorProfile = jobMap["colorProfile"].toString();

        if (loadInputImage(imagePath)) {
            // Only apply ICC conversion if the image is NOT already CMYK
            if (inputImage.colorSpace() != Magick::CMYKColorspace) {
                qDebug() << "Input is not CMYK — applying ICC conversion (sRGB → RIP CMYK)";

                QString inputICC = assetsExtractPath + "/sRGBProfile.icm";
                QString outputICC = defaultOutputICCPath;

                success = applyICCConversion(inputICC, outputICC);
                
            } else {
                qDebug() << "Input image is already CMYK — skipping ICC conversion.";
                success = true; // No need to convert
            }

            if (success) {
            	int xdpi = resolution.width();
                int ydpi = resolution.height();
				
                success = generateFinalPRN(outputPath, xdpi, ydpi);
            }
        }
        emit prnGenerationFinished(success);
    });
}


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


std::vector<std::vector<uint8_t>> PrintJobNocai::dotClassification(const std::vector<uint8_t>& dithered, const std::vector<uint8_t>& mask, int width, int height) {
    std::vector<std::vector<uint8_t>> dotMap(height, std::vector<uint8_t>(width, 0));

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (dithered[y * width + x] < 128) continue;

            uint8_t t = mask[y * width + x];
            if (t >= 192) dotMap[y][x] = 1;
            else if (t >= 128) dotMap[y][x] = 2;
            else dotMap[y][x] = 3;
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
        0x00005555,
        static_cast<uint32_t>(xdpi),
        static_cast<uint32_t>(ydpi),
        bytesPerLine,
        static_cast<uint32_t>(height),
        static_cast<uint32_t>(width),
        0, 4, 1, 1, 0, 0
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


void PrintJobNocai::setDefaultOutputICCProfile(const QString& outputProfile) {
    defaultOutputICCPath = outputProfile;
    qDebug() << "Default output ICC profile set to:" << outputProfile;
}


QString PrintJobNocai::getDefaultOutputICCProfile() const {
    return defaultOutputICCPath;
}


void PrintJobNocai::addICCProfile(const QString& name, const QString& path) {
    availableICCProfiles.append({name, path});
}


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

    // Blue Noise Masks (256x256 Tiles, 12000x12000 Size)
    copyIfMissing(":/assets/blue_noise_mask_256_12000/mask_c.tiff", assetsExtractPath + "/mask_256_c.tiff");
    copyIfMissing(":/assets/blue_noise_mask_256_12000/mask_m.tiff", assetsExtractPath + "/mask_256_m.tiff");
    copyIfMissing(":/assets/blue_noise_mask_256_12000/mask_y.tiff", assetsExtractPath + "/mask_256_y.tiff");
    copyIfMissing(":/assets/blue_noise_mask_256_12000/mask_k.tiff", assetsExtractPath + "/mask_256_k.tiff");
    
	auto addProfile = [&](const QString& name, const QString& qrcPath, const QString& fileName) {
		QString destPath = assetsExtractPath + "/" + fileName;
		if (!QFile::exists(destPath)) {
			QFile::copy(qrcPath, destPath);
		}
		addICCProfile(name, destPath);
	};

    // ICC Profiles
    addProfile("Plain Paper (720DPI)",  ":/assets/RIP_App_Plain_720.icm", "RIP_App_Plain_720.icm");
    addProfile("sRGB Input", ":/assets/sRGBProfile.icm", "sRGBProfile.icm");
    setDefaultOutputICCProfile((assetsExtractPath + "/RIP_App_Plain_720.icm"));

    qDebug() << "Nocai assets prepared in:" << assetsExtractPath;
}


void PrintJobNocai::cleanupTemporaryFiles(const QString& baseName, const QString& workingDir) {
    qDebug() << "🧹 Cleaning intermediate files for base:" << baseName << "in dir:" << workingDir;
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
        qDebug() << "✅ Working directory removed:" << workingDir;
    }
}


void PrintJobNocai::cleanupRuntimeAssets() {
    qDebug() << "🧹 Cleaning runtime assets in:" << assetsExtractPath;
    QDir dir(assetsExtractPath);
    if (dir.exists()) {
        dir.removeRecursively();
        qDebug() << "✅ Runtime assets cleaned.";
    } else {
        qDebug() << "⚠️ Runtime assets directory not found.";
    }
}
