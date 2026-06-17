#include "ColorProfile.h"

#include <QDebug>

namespace {
bool unavailable(const char* operation)
{
    qWarning() << operation << "requires the Android RIP/color dependency build.";
    return false;
}
}

ColorProfile::ColorProfile(QObject* parent)
    : QObject(parent)
{
}

bool ColorProfile::convertToColorspace(const QString& path, const QString& targetSpace)
{
    Q_UNUSED(path)
    Q_UNUSED(targetSpace)
    return unavailable("Colorspace conversion");
}

bool ColorProfile::loadProfiles(const QString& inputIccPath, const QString& outputIccPath)
{
    Q_UNUSED(inputIccPath)
    Q_UNUSED(outputIccPath)
    return unavailable("ICC profile loading");
}

bool ColorProfile::convertWithICCProfiles(const QString& imagePath, const QString& outputPath)
{
    Q_UNUSED(imagePath)
    Q_UNUSED(outputPath)
    return unavailable("ICC conversion");
}

bool ColorProfile::convertWithICCProfilesCMYK(const QString& imagePath, const QString& outputPath, const QString& inputICCPath, const QString& outputICCPath)
{
    Q_UNUSED(imagePath)
    Q_UNUSED(outputPath)
    Q_UNUSED(inputICCPath)
    Q_UNUSED(outputICCPath)
    return unavailable("CMYK ICC conversion");
}

bool ColorProfile::convertRgbToCmyk(const QString& imagePath) { Q_UNUSED(imagePath) return unavailable("RGB to CMYK conversion"); }
bool ColorProfile::convertCmykToRgb(const QString& imagePath) { Q_UNUSED(imagePath) return unavailable("CMYK to RGB conversion"); }
bool ColorProfile::convertToGrayscale(const QString& imagePath) { Q_UNUSED(imagePath) return unavailable("Grayscale conversion"); }
bool ColorProfile::convertToLcLmLyLk(const QString& imagePath) { Q_UNUSED(imagePath) return unavailable("Lc/Lm/Ly/Lk conversion"); }
bool ColorProfile::convertToIndexed(const QString& imagePath, bool useAnsi16)
{
    Q_UNUSED(imagePath)
    Q_UNUSED(useAnsi16)
    return unavailable("Indexed color conversion");
}
