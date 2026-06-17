// ColorProfile.h
// Qt-exposed helpers for colorspace conversion and ICC handling.
// Uses LittleCMS (lcms2) and Magick++ (in .cpp). Some methods overwrite files.

#include <QObject>
#include <QString>
#include <QVector>
#include <lcms2.h>

// Thin façade for QML calls; returns bool + logs on error (see .cpp).
// Not thread-safe; create per-thread instances if needed.
class ColorProfile : public QObject {
    Q_OBJECT

public:
    // Constructs an instance with no ICC profiles loaded. The destructor closes any open lcms profiles.
    explicit ColorProfile(QObject *parent = nullptr);
    ~ColorProfile();

    // Dispatch by token: "CMYK", "RGB"/"sRGB", "Grayscale", "Lc+Lm+Ly+Lk", "Indexed8", "Indexed16".
    // Note: several targets overwrite the file in place (see .cpp).
    Q_INVOKABLE bool convertToColorspace(const QString &path, const QString &targetSpace);

    // Load input/output ICC profiles for later transforms. Closes previous handles.
    Q_INVOKABLE bool loadProfiles(const QString &inputIccPath, const QString &outputIccPath);

    // LittleCMS RGB8->RGB8 transform using loaded profiles; writes to outputPath (non-destructive).
    Q_INVOKABLE bool convertWithICCProfiles(const QString &imagePath, const QString &outputPath);

    // Magick-driven apply(inputICC)->convert(outputICC)->CMYK TIFF write (non-destructive to source).
    Q_INVOKABLE bool convertWithICCProfilesCMYK(const QString &imagePath,
                                                const QString &outputPath,
                                                const QString &inputICCPath,
                                                const QString &outputICCPath);

    // The following conversions overwrite the input file on disk (destructive).
    Q_INVOKABLE bool convertRgbToCmyk(const QString &imagePath);     // Set CMYK and save.
    Q_INVOKABLE bool convertCmykToRgb(const QString &imagePath);     // Set RGB and save.
    Q_INVOKABLE bool convertToGrayscale(const QString &imagePath);   // Set grayscale and save.
    Q_INVOKABLE bool convertToLcLmLyLk(const QString &imagePath);    // Write 5-tile montage (preview aid).
    Q_INVOKABLE bool convertToIndexed(const QString &imagePath, bool useAnsi16); // Quantize to 8/16 colors.

private:
    // LittleCMS handles used by convertWithICCProfiles(...). Null if not loaded.
    cmsHPROFILE inputProfile_ = nullptr;
    cmsHPROFILE outputProfile_ = nullptr;

    // Fixed sRGB palettes: 8-color or ANSI-like 16-color.
    QVector<QVector<uchar>> getPalette(bool useAnsi16) const;

    // Nearest palette color by squared RGB distance (use Lab for better perceptual results if needed).
    int findNearestColorIndex(const QVector<uchar> &rgb,
                              const QVector<QVector<uchar>> &palette) const;
};

