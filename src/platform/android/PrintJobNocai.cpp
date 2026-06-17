#include "PrintJobNocai.h"

#include <QDebug>
#include <QDir>

namespace {
bool unavailable(const char* operation)
{
    qWarning() << operation << "requires the Android RIP dependency build.";
    return false;
}
}

PrintJobNocai::PrintJobNocai(QObject* parent)
    : QObject(parent)
{
}

void PrintJobNocai::setColorManager(ColorManagementManager* mgr)
{
    m_colorManager = mgr;
}

void PrintJobNocai::runPRNGeneration(const QVariantMap& jobMap, const QString& outputPath)
{
    Q_UNUSED(jobMap)
    Q_UNUSED(outputPath)
    unavailable("Nocai PRN generation");
    emit prnGenerationFinished(false);
}

bool PrintJobNocai::loadInputImage(const QString& imagePath) { Q_UNUSED(imagePath) return unavailable("Image loading for PRN"); }
bool PrintJobNocai::applyICCConversion(const QString& inputProfile, const QString& outputProfile) { Q_UNUSED(inputProfile) Q_UNUSED(outputProfile) return unavailable("ICC conversion for PRN"); }
bool PrintJobNocai::generateFinalPRN(const QString& outputPath, int xdpi, int ydpi) { Q_UNUSED(outputPath) Q_UNUSED(xdpi) Q_UNUSED(ydpi) return unavailable("Final PRN generation"); }

void PrintJobNocai::prepareNocaiAssets()
{
    if (!m_profiles.isEmpty())
        return;

    addICCProfile(QStringLiteral("Default - Plain Paper (1440DPI)"), QStringLiteral(":/assets/RIP_App_1440_Plain_Default.icc"));
    addICCProfile(QStringLiteral("Neutral Profile - Plain Paper (1440DPI)"), QStringLiteral(":/assets/RIP_App_1440_Plain_Neutral.icc"));
    addICCProfile(QStringLiteral("sRGB Input"), QStringLiteral(":/assets/sRGBProfile.icm"));
    addICCProfile(QStringLiteral("CMYK Input"), QStringLiteral(":/assets/RIP_App_Generic_CMYK.icc"));
    if (m_defaultOutputICCPath.isEmpty())
        m_defaultOutputICCPath = QStringLiteral(":/assets/RIP_App_1440_Plain_Default.icc");
    if (m_defaultInputCMYKPath.isEmpty())
        m_defaultInputCMYKPath = QStringLiteral(":/assets/RIP_App_Generic_CMYK.icc");
}

void PrintJobNocai::cleanupTemporaryFiles(const QString& baseName, const QString& workingDir)
{
    Q_UNUSED(baseName)
    Q_UNUSED(workingDir)
}

void PrintJobNocai::cleanupRuntimeAssets()
{
}

QVariantList PrintJobNocai::getAvailableICCProfiles() const { return m_profiles; }
QString PrintJobNocai::getDefaultOutputICCProfile() const { return m_defaultOutputICCPath; }
QString PrintJobNocai::getDefaultInputCMYKProfile() const { return m_defaultInputCMYKPath; }
void PrintJobNocai::setDefaultOutputICCProfile(const QString& outputProfile) { m_defaultOutputICCPath = outputProfile; }
void PrintJobNocai::setDefaultInputCMYKProfile(const QString& inputProfilePath) { m_defaultInputCMYKPath = inputProfilePath; }
void PrintJobNocai::enableDefaultInputCMYK(bool enabled) { m_useDefaultInputCMYK = enabled; }
bool PrintJobNocai::checkDefaultInputCMYK() const { return m_useDefaultInputCMYK; }

void PrintJobNocai::addICCProfile(const QString& name, const QString& path)
{
    QVariantMap profile;
    profile["name"] = name;
    profile["path"] = path;
    m_profiles.append(profile);
}

void PrintJobNocai::setDotStrategy(int minInkThreshold,
                                   int smallDotThreshold,
                                   int medDotThreshold,
                                   bool enablePromotion,
                                   uint8_t floorRangeCMY,
                                   uint8_t floorMaxCMY,
                                   uint8_t floorRangeK,
                                   uint8_t floorMaxK,
                                   bool enableDotSwap)
{
    Q_UNUSED(minInkThreshold)
    Q_UNUSED(smallDotThreshold)
    Q_UNUSED(medDotThreshold)
    Q_UNUSED(enablePromotion)
    Q_UNUSED(floorRangeCMY)
    Q_UNUSED(floorMaxCMY)
    Q_UNUSED(floorRangeK)
    Q_UNUSED(floorMaxK)
    Q_UNUSED(enableDotSwap)
}
