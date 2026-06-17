#include "ImageLoader.h"

#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QUrl>

namespace {
QString localPathFor(const QString& path)
{
    return path.startsWith(QStringLiteral("file:"), Qt::CaseInsensitive)
        ? QUrl(path).toLocalFile()
        : path;
}
}

ImageLoader::ImageLoader(QObject* parent)
    : QObject(parent)
{
}

QString ImageLoader::renderPdfToPreviewImage(const QString& pdfPath)
{
    Q_UNUSED(pdfPath)
    return {};
}

void ImageLoader::deleteTemporaryFile(const QString& path)
{
    const QString localPath = localPathFor(path);
    if (!localPath.isEmpty())
        QFile::remove(localPath);
}

bool ImageLoader::validateFile(const QString& path)
{
    const QString localPath = localPathFor(path);
    return QFileInfo::exists(localPath) && isSupportedExtension(localPath);
}

QVariantMap ImageLoader::extractMetadata(const QString& path)
{
    const QString localPath = localPathFor(path);
    QFileInfo info(localPath);
    QVariantMap metadata;
    metadata["fileName"] = info.fileName();
    metadata["fileSize"] = info.size();
    metadata["filePath"] = localPath;

    QImageReader reader(localPath);
    if (reader.canRead()) {
        const QSize size = reader.size();
        metadata["width"] = size.width();
        metadata["height"] = size.height();
        metadata["format"] = QString::fromLatin1(reader.format());
    }

    return metadata;
}

bool ImageLoader::isSupportedExtension(const QString& path)
{
    const QString suffix = QFileInfo(localPathFor(path)).suffix().toLower();
    return suffix == "jpg" || suffix == "jpeg" || suffix == "png" ||
           suffix == "bmp" || suffix == "tif" || suffix == "tiff";
}
