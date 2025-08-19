#include <QObject>
#include <QString>
#include <QVariantMap>

// ImageLoader.h
// Validation and metadata extraction for bitmap images, SVG, and PDF.
// Also renders a lightweight preview image for PDFs.

/*
 * ImageLoader — Qt-exposed helpers for file validation, metadata, and previews.
 * Methods return bool/empty values on failure and log details in the .cpp.
 */
class ImageLoader : public QObject {
    Q_OBJECT

public:
    explicit ImageLoader(QObject *parent = nullptr);

	// Render first page of a PDF to a preview image. Returns file path or "" on failure.
    Q_INVOKABLE QString renderPdfToPreviewImage(const QString& pdfPath);
    
    // Delete a temporary/preview file if it exists (utility).
    Q_INVOKABLE void deleteTemporaryFile(const QString& path);

	// Validate if file is supported and loadable
    Q_INVOKABLE bool validateFile(const QString &path);

    // Extract metadata from supported file
    Q_INVOKABLE QVariantMap extractMetadata(const QString &path);

    // Check file extension support
    Q_INVOKABLE bool isSupportedExtension(const QString &path);

private:
	
	// Accepted extensions, e.g., jpg/jpeg/png/bmp/tif/tiff/svg/pdf (populated in .cpp).
    QStringList supportedExtensions;

    // Internal helpers
    QString getFileExtension(const QString &path);		// Returns normalized lowercase extension without dot.
    QVariantMap inspectImage(const QString &path);		// Bitmap metadata (dims, depth, colorspace).
    QVariantMap inspectSvgOrPdf(const QString &path);	// Vector/PDF metadata (page size, pages, etc.).
};
