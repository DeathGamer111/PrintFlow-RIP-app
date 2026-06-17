#include "AssetManager.h"

#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDebug>

bool AssetManager::initialize(const QString& subdir)
{
    m_rootPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (m_rootPath.isEmpty()) {
        qWarning() << "AssetManager: AppDataLocation is empty.";
        return false;
    }

    if (!subdir.trimmed().isEmpty()) {
        m_rootPath += "/" + subdir;
    }

    if (!QDir().mkpath(m_rootPath)) {
        qWarning() << "AssetManager: failed to create asset directory:" << m_rootPath;
        return false;
    }

    return true;
}

QString AssetManager::rootPath() const
{
    return m_rootPath;
}

QString AssetManager::assetPath(const QString& fileName) const
{
    if (m_rootPath.isEmpty())
        return QString();
    return m_rootPath + "/" + fileName;
}

bool AssetManager::hasAsset(const QString& fileName) const
{
    const QString p = assetPath(fileName);
    return !p.isEmpty() && QFileInfo::exists(p);
}

bool AssetManager::copyResourceIfMissing(const QString& resourcePath, const QString& fileName)
{
    if (m_rootPath.isEmpty()) {
        qWarning() << "AssetManager: initialize() must be called first.";
        return false;
    }

    const QString dest = assetPath(fileName);
    if (QFile::exists(dest))
        return true;

    if (!QFile::exists(resourcePath)) {
        qWarning() << "AssetManager: missing resource:" << resourcePath;
        return false;
    }

    if (!QFile::copy(resourcePath, dest)) {
        qWarning() << "AssetManager: failed to copy" << resourcePath << "to" << dest;
        return false;
    }

    return true;
}

bool AssetManager::copyResourcesIfMissing(const QStringList& resourcePaths, const QStringList& fileNames)
{
    if (resourcePaths.size() != fileNames.size()) {
        qWarning() << "AssetManager: resource/file list size mismatch.";
        return false;
    }

    for (int i = 0; i < resourcePaths.size(); ++i) {
        if (!copyResourceIfMissing(resourcePaths[i], fileNames[i])) {
            return false;
        }
    }

    return true;
}

bool AssetManager::cleanup()
{
    if (m_rootPath.isEmpty())
        return true;

    QDir dir(m_rootPath);
    if (!dir.exists())
        return true;

    if (!dir.removeRecursively()) {
        qWarning() << "AssetManager: failed to remove asset directory:" << m_rootPath;
        return false;
    }

    return true;
}
