#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>

class ImageLoader : public QObject
{
    Q_OBJECT

public:
    explicit ImageLoader(QObject* parent = nullptr);

    Q_INVOKABLE QString renderPdfToPreviewImage(const QString& pdfPath);
    Q_INVOKABLE void deleteTemporaryFile(const QString& path);
    Q_INVOKABLE bool validateFile(const QString& path);
    Q_INVOKABLE QVariantMap extractMetadata(const QString& path);
    Q_INVOKABLE bool isSupportedExtension(const QString& path);
};
