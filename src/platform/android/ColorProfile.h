#pragma once

#include <QObject>
#include <QString>

class ColorProfile : public QObject
{
    Q_OBJECT

public:
    explicit ColorProfile(QObject* parent = nullptr);
    ~ColorProfile() override = default;

    Q_INVOKABLE bool convertToColorspace(const QString& path, const QString& targetSpace);
    Q_INVOKABLE bool loadProfiles(const QString& inputIccPath, const QString& outputIccPath);
    Q_INVOKABLE bool convertWithICCProfiles(const QString& imagePath, const QString& outputPath);
    Q_INVOKABLE bool convertWithICCProfilesCMYK(const QString& imagePath,
                                                const QString& outputPath,
                                                const QString& inputICCPath,
                                                const QString& outputICCPath);
    Q_INVOKABLE bool convertRgbToCmyk(const QString& imagePath);
    Q_INVOKABLE bool convertCmykToRgb(const QString& imagePath);
    Q_INVOKABLE bool convertToGrayscale(const QString& imagePath);
    Q_INVOKABLE bool convertToLcLmLyLk(const QString& imagePath);
    Q_INVOKABLE bool convertToIndexed(const QString& imagePath, bool useAnsi16);
};
