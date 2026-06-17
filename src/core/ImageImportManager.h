#pragma once

#include <QObject>
#include <QString>

#if defined(Q_OS_ANDROID)
#include <QtCore/private/qandroidextras_p.h>
#endif

class ImageImportManager
    : public QObject
#if defined(Q_OS_ANDROID)
    , public QAndroidActivityResultReceiver
#endif
{
    Q_OBJECT
    Q_PROPERTY(bool supportsNativeImagePicker READ supportsNativeImagePicker CONSTANT)
    Q_PROPERTY(bool supportsCamera READ supportsCamera CONSTANT)

public:
    explicit ImageImportManager(QObject *parent = nullptr);

    bool supportsNativeImagePicker() const;
    bool supportsCamera() const;

    Q_INVOKABLE void openImageImportChooser();
    Q_INVOKABLE void openImagePicker();
    Q_INVOKABLE void openCamera();

signals:
    void imageReady(const QString &localFilePath);
    void canceled();
    void failed(const QString &message);

#if defined(Q_OS_ANDROID)
public:
    void handleActivityResult(int receiverRequestCode, int resultCode, const QJniObject &data) override;

private:
    QString copyUriToLocalFile(const QJniObject &uri, const QString &fallbackExtension);
    QString displayNameForUri(const QJniObject &uri) const;
    QString saveCameraBitmap(const QJniObject &bitmap);
    QString newImportPath(const QString &extension, const QString &baseName = QString()) const;
#endif
};
