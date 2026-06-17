#include "ColorProfile.h"
#include <Magick++.h>
#include <QFile>
#include <QUrl>
#include <QDebug>
#include <fstream>


// ColorProfile.cpp — Magick++ for I/O/basic colorspace; LittleCMS for ICC transforms. Methods log and return false on failure.
ColorProfile::ColorProfile(QObject *parent) : QObject(parent) {}  // No ICC profiles loaded initially.


ColorProfile::~ColorProfile() {
    if (inputProfile_)  cmsCloseProfile(inputProfile_);   // Close input ICC if opened.
    if (outputProfile_) cmsCloseProfile(outputProfile_);  // Close output ICC if opened.
}


bool ColorProfile::convertToColorspace(const QString &imagePath, const QString &colorspace) {
    // Dispatch by token; several branches overwrite the file (see callee docs).
    if (colorspace == "CMYK") {
        return convertRgbToCmyk(imagePath);
    } else if (colorspace == "RGB" || colorspace == "sRGB") {
        return convertCmykToRgb(imagePath);
    } else if (colorspace == "Grayscale") {
        return convertToGrayscale(imagePath);
    } else if (colorspace == "Lc+Lm+Ly+Lk") {
        return convertToLcLmLyLk(imagePath);
    } else if (colorspace == "Indexed8") {
        return convertToIndexed(imagePath, false);
    } else if (colorspace == "Indexed16") {
        return convertToIndexed(imagePath, true);
    }
    return false; // Unknown token.
}


bool ColorProfile::loadProfiles(const QString &inputIccPath, const QString &outputIccPath) {
    // Close any previously loaded profiles before reopening.
    if (inputProfile_) cmsCloseProfile(inputProfile_);
    if (outputProfile_) cmsCloseProfile(outputProfile_);

    inputProfile_  = cmsOpenProfileFromFile(qPrintable(inputIccPath), "r");  // Source profile.
    outputProfile_ = cmsOpenProfileFromFile(qPrintable(outputIccPath), "r"); // Destination profile.

    if (!inputProfile_ || !outputProfile_) {
        qWarning() << "Failed to open ICC profiles.";
        return false;
    }

    qDebug() << "ICC profiles loaded successfully.";
    return true;
}


bool ColorProfile::convertWithICCProfiles(const QString &imagePath, const QString &outputPath) {
    // LittleCMS RGB8->RGB8 transform using loaded profiles; writes to outputPath.
    if (!inputProfile_ || !outputProfile_) {
        qWarning() << "ICC profiles not loaded.";
        return false;
    }

    try {
        QString inLocal  = QUrl(imagePath).toLocalFile();
        QString outLocal = QUrl(outputPath).toLocalFile();

        Magick::Image image(inLocal.toStdString());
        image.type(Magick::TrueColorType);
        image.colorSpace(Magick::RGBColorspace);  // Normalize to RGB for LCMS.

        int width  = image.columns();
        int height = image.rows();
        std::vector<uchar> inputPixels(width * height * 3);   // RGB input buffer.
        std::vector<uchar> outputPixels(width * height * 3);  // RGB output buffer.

        image.write(0, 0, width, height, "RGB", Magick::CharPixel, inputPixels.data()); // Read pixels.

        cmsHTRANSFORM transform = cmsCreateTransform(
            inputProfile_, TYPE_RGB_8,
            outputProfile_, TYPE_RGB_8,
            INTENT_PERCEPTUAL, 0); // Perceptual intent; adjust if proofing is required.

        if (!transform) {
            qWarning() << "Failed to create ICC transform.";
            return false;
        }

        cmsDoTransform(transform, inputPixels.data(), outputPixels.data(), width * height); // Apply transform.
        cmsDeleteTransform(transform);

        Magick::Image outputImage(Magick::Geometry(width, height), "white");
        outputImage.type(Magick::TrueColorType);
        outputImage.colorSpace(Magick::RGBColorspace);
        outputImage.read(width, height, "RGB", Magick::CharPixel, outputPixels.data()); // Rebuild image.

        outputImage.write(outLocal.toStdString()); // Non-destructive: writes to outputPath.
        return true;

    } catch (const Magick::Exception &e) {
        qWarning() << "ICC conversion failed:" << e.what();
        return false;
    }
}


bool ColorProfile::convertWithICCProfilesCMYK(const QString &imagePath, const QString &outputPath, const QString &inputICCPath, const QString &outputICCPath) {
    // Magick-driven: strip profiles -> attach input -> attach output (convert) -> set CMYK -> write TIFF.
    try {
        QString inLocal   = QUrl(imagePath).toLocalFile();
        QString outLocal  = QUrl(outputPath).toLocalFile();
        QString inputICC  = QUrl(inputICCPath).toLocalFile();
        QString outputICC = QUrl(outputICCPath).toLocalFile();

        Magick::Image image(inLocal.toStdString());

        image.profile("icc", Magick::Blob()); // Remove any embedded profile to avoid conflicts.

        std::ifstream inProfileFile(inputICC.toStdString(), std::ios::binary);
        if (!inProfileFile) {
            qWarning() << "Failed to open input ICC profile:" << inputICC;
            return false;
        }
        std::vector<char> inProfileData((std::istreambuf_iterator<char>(inProfileFile)), {});
        image.profile("icc", Magick::Blob(inProfileData.data(), inProfileData.size())); // Attach source profile.

        std::ifstream outProfileFile(outputICC.toStdString(), std::ios::binary);
        if (!outProfileFile) {
            qWarning() << "Failed to open output ICC profile:" << outputICC;
            return false;
        }
        std::vector<char> outProfileData((std::istreambuf_iterator<char>(outProfileFile)), {});
        image.profile("icc", Magick::Blob(outProfileData.data(), outProfileData.size())); // Convert to dest profile.

        image.colorSpace(Magick::CMYKColorspace);  // Ensure CMYK storage.
        image.magick("TIFF");
        image.depth(8);
        image.defineValue("tiff:bits-per-sample", "8"); // 8 bpc for compatibility.

        image.write(outLocal.toStdString()); // Non-destructive to source file.

        qDebug() << "ICC CMYK conversion succeeded. Output saved to:" << outLocal;
        return true;

    } catch (const Magick::Exception& e) {
        qWarning() << "ICC conversion failed:" << e.what();
        return false;
    }
}


bool ColorProfile::convertRgbToCmyk(const QString &imagePath) {
    // DESTRUCTIVE: overwrites the input file with CMYK colorspace.
    try {
        QString localPath = QUrl(imagePath).toLocalFile();
        Magick::Image image(localPath.toStdString());
        image.colorSpace(Magick::CMYKColorspace);
        image.write(localPath.toStdString());
        return true;
    } catch (std::exception &e) {
        qWarning() << "Error in convertRgbToCmyk:" << e.what();
        return false;
    }
}


bool ColorProfile::convertCmykToRgb(const QString &imagePath) {
    // DESTRUCTIVE: overwrites the input file with RGB colorspace.
    try {
        QString localPath = QUrl(imagePath).toLocalFile();
        Magick::Image image(localPath.toStdString());
        image.colorSpace(Magick::RGBColorspace);
        image.write(localPath.toStdString());
        return true;
    } catch (std::exception &e) {
        qWarning() << "Error in convertCmykToRgb:" << e.what();
        return false;
    }
}


bool ColorProfile::convertToGrayscale(const QString &imagePath) {
    // DESTRUCTIVE: overwrites the input file as grayscale.
    try {
        QString localPath = QUrl(imagePath).toLocalFile();
        Magick::Image image(localPath.toStdString());
        image.type(Magick::GrayscaleType);
        image.write(localPath.toStdString());
        return true;
    } catch (std::exception &e) {
        qWarning() << "Error in grayscale conversion:" << e.what();
        return false;
    }
}


bool ColorProfile::convertToLcLmLyLk(const QString &imagePath) {
    // DESTRUCTIVE: writes a 5-tile montage (base CMYK + heuristic “light” variants).
    try {
        QString localPath = QUrl(imagePath).toLocalFile();
        Magick::Image base(localPath.toStdString());
        base.colorSpace(Magick::CMYKColorspace);

        Magick::Image lightCyan    = base;
        Magick::Image lightMagenta = base;
        Magick::Image lightYellow  = base;
        Magick::Image lightBlack   = base;

        lightCyan.modulate(100, 50, 100);    // Reduce saturation as a rough “light” approximation.
        lightMagenta.modulate(100, 50, 100);
        lightYellow.modulate(100, 50, 100);
        lightBlack.level(0.0, 0.5);          // Lift blacks for a “light K” preview.

        int width       = base.columns();
        int height      = base.rows();
        int totalWidth  = width * 5;

        Magick::Image montage(Magick::Geometry(totalWidth, height), "white");
        montage.type(Magick::TrueColorType);

        montage.composite(base,         0 * width, 0, Magick::OverCompositeOp);
        montage.composite(lightCyan,    1 * width, 0, Magick::OverCompositeOp);
        montage.composite(lightMagenta, 2 * width, 0, Magick::OverCompositeOp);
        montage.composite(lightYellow,  3 * width, 0, Magick::OverCompositeOp);
        montage.composite(lightBlack,   4 * width, 0, Magick::OverCompositeOp);

        montage.write(localPath.toStdString());
        return true;

    } catch (const Magick::Exception &e) {
        qWarning() << "Error in convertToLcLmLyLk:" << e.what();
        return false;
    }
}


bool ColorProfile::convertToIndexed(const QString &imagePath, bool useAnsi16) {
    // DESTRUCTIVE: quantize to 8 or 16 colors using Magick’s quantizer.
    try {
        QString localPath = QUrl(imagePath).toLocalFile();
        Magick::Image image(localPath.toStdString());
        image.quantizeColorSpace(Magick::RGBColorspace);        // Work in RGB for palette logic.
        image.quantizeColors(useAnsi16 ? 16 : 8);               // Target palette size.
        image.quantize();                                       // Execute quantization.
        image.write(localPath.toStdString());
        return true;
    } catch (std::exception &e) {
        qWarning() << "Error in indexed color conversion:" << e.what();
        return false;
    }
}


QVector<QVector<uchar>> ColorProfile::getPalette(bool useAnsi16) const {
    // Fixed sRGB palettes (8-color or ANSI-like 16-color).
    if (useAnsi16) {
        return {
            {0,0,0}, {0,0,170}, {0,170,0}, {0,170,170},
            {170,0,0}, {170,0,170}, {170,85,0}, {170,170,170},
            {85,85,85}, {85,85,255}, {85,255,85}, {85,255,255},
            {255,85,85}, {255,85,255}, {255,255,85}, {255,255,255}
        };
    } else {
        return {
            {0,0,0}, {128,0,0}, {0,128,0}, {128,128,0},
            {0,0,128}, {128,0,128}, {0,128,128}, {192,192,192}
        };
    }
}


int ColorProfile::findNearestColorIndex(const QVector<uchar> &rgb, const QVector<QVector<uchar>> &palette) const {
    // Nearest neighbor in squared RGB space; replace with Lab for perceptual accuracy if needed.
    int bestIndex = 0;
    int minDist = INT32_MAX;

    for (int i = 0; i < palette.size(); ++i) {
        int dr = rgb[0] - palette[i][0];
        int dg = rgb[1] - palette[i][1];
        int db = rgb[2] - palette[i][2];
        int dist = dr * dr + dg * dg + db * db;

        if (dist < minDist) {
            minDist = dist;
            bestIndex = i;
        }
    }
    return bestIndex;
}

