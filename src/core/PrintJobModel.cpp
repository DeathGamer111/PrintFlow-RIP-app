#include "PrintJobModel.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime> 


/*********************************************************************************************
    PrintJobModel constructor, 
    QAbstractListModel storing print jobs; JSON import/export with optional embedded image data.
**********************************************************************************************/
PrintJobModel::PrintJobModel(QObject *parent) : QAbstractListModel(parent) {}


// Return number of jobs in the model
int PrintJobModel::rowCount(const QModelIndex &) const {
    return m_jobs.count();
}


// Return data for a given job and role
QVariant PrintJobModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_jobs.size()) return QVariant();
    const PrintJob &job = m_jobs.at(index.row());
    switch (role) {
    case IdRole: return job.id;
    case NameRole: return job.name;
    case ImagePathRole: return job.imagePath;
    case PaperSizeRole: return QVariant::fromValue(job.paperSize);
    case ResolutionRole: return QVariant::fromValue(job.resolution);
    case OffsetRole: return QVariant::fromValue(job.offset);
    case WhiteStrategyRole: return job.whiteStrategy;
    case VarnishTypeRole: return job.varnishType;
    case ColorProfileRole: return job.colorProfile;
    case CreatedAtRole: return job.createdAt;
    case WhitePlatePathRole: return job.whitePlatePath;
    case VarnishPlatePathRole: return job.varnishPlatePath;
    default: return QVariant();
    }
}


// Map internal role IDs to role names for QML
QHash<int, QByteArray> PrintJobModel::roleNames() const {
    return {
        {IdRole, "id"},
        {NameRole, "name"},
        {ImagePathRole, "imagePath"},
        {PaperSizeRole, "paperSize"},
        {ResolutionRole, "resolution"},
        {OffsetRole, "offset"},
        {WhiteStrategyRole, "whiteStrategy"},
        {VarnishTypeRole, "varnishType"},
        {ColorProfileRole, "colorProfile"},
        {CreatedAtRole, "createdAt"},
        {WhitePlatePathRole, "whitePlatePath"},
        {VarnishPlatePathRole, "varnishPlatePath"}
    };
}


// Add a new print job with default values
void PrintJobModel::addJob(const QString &name) {
    beginInsertRows(QModelIndex(), m_jobs.size(), m_jobs.size());
    PrintJob job;
    job.id = QString::number(QDateTime::currentMSecsSinceEpoch());
    job.name = name;
    job.createdAt = QDateTime::currentDateTime();
    job.paperSize = QSize(210, 297);    // Default A4 Paper Size
    job.resolution = QSize(720, 1440);	// RIP default DPI.
    job.offset = QPoint(0, 0);
    job.whiteStrategy = "None";
    job.varnishType = "None";
    job.colorProfile = "sRGB";
    job.whitePlatePath = "";
    job.varnishPlatePath = "";
    m_jobs.append(job);
    endInsertRows();
    emit countChanged();
}


// Remove a job by index position
void PrintJobModel::removeJob(int index) {
    if (index < 0 || index >= m_jobs.size()) return;
    beginRemoveRows(QModelIndex(), index, index);
    m_jobs.removeAt(index);
    endRemoveRows();
    emit countChanged();
}


// Return a print job as a QVariantMap
QVariantMap PrintJobModel::getJob(int index) const {
    QVariantMap map;
    if (index < 0 || index >= m_jobs.size()) return map;
    const PrintJob &job = m_jobs.at(index);
    map["id"] = job.id;
    map["name"] = job.name;
    map["imagePath"] = job.imagePath;
    map["paperSize"] = QVariant::fromValue(job.paperSize);
    map["resolution"] = QVariant::fromValue(job.resolution);
    map["offset"] = QVariant::fromValue(job.offset);
    map["whiteStrategy"] = job.whiteStrategy;
    map["varnishType"] = job.varnishType;
    map["colorProfile"] = job.colorProfile;
    map["createdAt"] = job.createdAt;
    map["whitePlatePath"] = job.whitePlatePath;
    map["varnishPlatePath"] = job.varnishPlatePath;
        
    return map;
}


// Update a print job from a QVariantMap
void PrintJobModel::updateJob(int index, const QVariantMap &jobData) {
    if (index < 0 || index >= m_jobs.size()) return;

    PrintJob &job = m_jobs[index];

    // Only update fields that are present, so partial updates don't wipe values.
    if (jobData.contains("name"))          job.name = jobData.value("name").toString();
    if (jobData.contains("imagePath"))     job.imagePath = jobData.value("imagePath").toString();

    if (jobData.contains("paperSize"))     job.paperSize = jobData.value("paperSize").toSize();
    if (jobData.contains("resolution"))    job.resolution = jobData.value("resolution").toSize();
    if (jobData.contains("offset"))        job.offset = jobData.value("offset").toPoint();

    if (jobData.contains("whiteStrategy")) job.whiteStrategy = jobData.value("whiteStrategy").toString();
    if (jobData.contains("varnishType"))   job.varnishType = jobData.value("varnishType").toString();
    if (jobData.contains("colorProfile"))  job.colorProfile = jobData.value("colorProfile").toString();
    
    if (jobData.contains("whitePlatePath"))    job.whitePlatePath = jobData.value("whitePlatePath").toString();
    if (jobData.contains("varnishPlatePath"))  job.varnishPlatePath = jobData.value("varnishPlatePath").toString();

    // Optional: allow createdAt updates if you ever want it from QML.
    if (jobData.contains("createdAt"))     job.createdAt = jobData.value("createdAt").toDateTime();

    emit dataChanged(this->index(index), this->index(index));
}


// Load jobs from a JSON file (with optional embedded images)
void PrintJobModel::loadFromJson(const QString &filePath) {
    const QString localPath = QUrl(filePath).toLocalFile();
    qDebug() << "[LOAD JSON]" << localPath;

    QFile file(localPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open file for reading:" << localPath;
        return;
    }

    const QByteArray jsonData = file.readAll();
    file.close();

    const QJsonDocument doc = QJsonDocument::fromJson(jsonData);
    QList<PrintJob> newJobs;

    auto parseObject = [&](const QJsonObject &obj) {
        PrintJob job;
        job.id = obj["id"].toString();
        job.name = obj["name"].toString();
        job.imagePath = obj["imagePath"].toString();  // May be overwritten below

        job.paperSize = QSize(obj["paperSizeWidth"].toInt(), obj["paperSizeHeight"].toInt());
        job.resolution = QSize(obj["resolutionWidth"].toInt(), obj["resolutionHeight"].toInt());
        job.offset.setX(obj["offsetX"].toInt());
        job.offset.setY(obj["offsetY"].toInt());

        job.whiteStrategy = obj["whiteStrategy"].toString();
        job.varnishType = obj["varnishType"].toString();
        job.colorProfile = obj["colorProfile"].toString();
        
		job.whitePlatePath = obj["whitePlatePath"].toString();
		job.varnishPlatePath = obj["varnishPlatePath"].toString();

        job.createdAt = QDateTime::fromString(obj["createdAt"].toString(), Qt::ISODate);
        if (!job.createdAt.isValid()) {
            // Backward compatibility / missing field
            job.createdAt = QDateTime::currentDateTime();
        }

        // If image data is embedded, reconstruct the image file
        if (obj.contains("imageData")) {
            const QByteArray imageData = QByteArray::fromBase64(obj["imageData"].toString().toUtf8());

            QString originalExt = QFileInfo(job.imagePath).suffix();
            if (originalExt.isEmpty())
                originalExt = "png";

            const QString newPath = QFileInfo(localPath).absoluteDir().filePath(job.id + "." + originalExt);

            QFile outImage(newPath);
            if (outImage.open(QIODevice::WriteOnly)) {
                outImage.write(imageData);
                outImage.close();
                job.imagePath = QUrl::fromLocalFile(newPath).toString();
            }
        }

        // If ID is missing, generate one (defensive)
        if (job.id.isEmpty())
            job.id = QString::number(QDateTime::currentMSecsSinceEpoch());

        newJobs.append(job);
    };

    if (doc.isArray()) {
        const QJsonArray arr = doc.array();
        for (const QJsonValue &val : arr) {
            if (val.isObject())
                parseObject(val.toObject());
        }
    }
    else if (doc.isObject()) {
        parseObject(doc.object());
    }
    else {
        qWarning() << "Invalid JSON structure in" << filePath;
        return;
    }

    if (newJobs.isEmpty()) {
        qDebug() << "[LOAD JSON] No jobs found in" << localPath;
        return;
    }

    const int start = m_jobs.size();
    const int end = start + newJobs.size() - 1;

    beginInsertRows(QModelIndex(), start, end);
    m_jobs.append(newJobs);
    endInsertRows();
    emit countChanged();
}


// Save selected jobs to JSON file, embedding image data as base64
void PrintJobModel::saveToJson(const QString &filePath, const QList<int> &selectedIndexes) {
    const QString localPath = QUrl(filePath).toLocalFile();

    QFile file(localPath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to open file for writing:" << localPath;
        return;
    }

    QJsonArray array;
    for (int index : selectedIndexes) {
        if (index < 0 || index >= m_jobs.size()) continue;
        const PrintJob &job = m_jobs[index];

        QJsonObject obj;
        obj["id"] = job.id;
        obj["name"] = job.name;
        obj["imagePath"] = job.imagePath;
        obj["paperSizeWidth"] = job.paperSize.width();
        obj["paperSizeHeight"] = job.paperSize.height();
        obj["resolutionWidth"] = job.resolution.width();
        obj["resolutionHeight"] = job.resolution.height();
        obj["offsetX"] = job.offset.x();
        obj["offsetY"] = job.offset.y();
        obj["whiteStrategy"] = job.whiteStrategy;
        obj["varnishType"] = job.varnishType;
        obj["colorProfile"] = job.colorProfile;
        obj["createdAt"] = job.createdAt.toString(Qt::ISODate);
        obj["whitePlatePath"] = job.whitePlatePath;
        obj["varnishPlatePath"] = job.varnishPlatePath;
                

        // Embed image base64 if available
        QFile imageFile(QUrl(job.imagePath).toLocalFile());
        if (imageFile.open(QIODevice::ReadOnly)) {
            QByteArray imageData = imageFile.readAll();
            obj["imageData"] = QString::fromUtf8(imageData.toBase64());
            imageFile.close();
        }

        array.append(obj);
    }

    QJsonDocument doc(array);
    file.write(doc.toJson());
    file.close();
}
