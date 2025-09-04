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
    case MinInkThresholdRole: return job.minInkThreshold;
	case SmallDotThresholdRole: return job.smallDotThreshold;
	case MedDotThresholdRole: return job.medDotThreshold;
	case EnablePromotionRole: return job.enablePromotion;
	case FloorRangeCMYRole: return job.floorRangeCMY;
	case FloorMaxCMYRole:   return job.floorMaxCMY;
	case FloorRangeKRole:   return job.floorRangeK;
	case FloorMaxKRole:     return job.floorMaxK;
	case EnableDotSwapRole: return job.enableDotSwap;
    case CreatedAtRole: return job.createdAt;
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
        {MinInkThresholdRole, "minInkThreshold"},
		{SmallDotThresholdRole, "smallDotThreshold"},
		{MedDotThresholdRole, "medDotThreshold"},
		{EnablePromotionRole, "enablePromotion"},
		{FloorRangeCMYRole, "floorRangeCMY" },
		{FloorMaxCMYRole, "floorMaxCMY" },
		{FloorRangeKRole, "floorRangeK" },
		{FloorMaxKRole, "floorMaxK" },
		{EnableDotSwapRole, "enableDotSwap" },
        {CreatedAtRole, "createdAt"}
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
    job.minInkThreshold = 8;
    job.smallDotThreshold = 96;
    job.medDotThreshold = 160;
    job.enablePromotion = false;
    job.floorRangeCMY = 24;
    job.floorMaxCMY = 2;
    job.floorRangeK = 12;
    job.floorMaxK = 0;
    job.enableDotSwap = false;
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
    map["minInkThreshold"] = job.minInkThreshold;
	map["smallDotThreshold"] = job.smallDotThreshold;
	map["medDotThreshold"] = job.medDotThreshold;
	map["enablePromotion"] = job.enablePromotion;
	map["floorRangeCMY"] = job.floorRangeCMY;
	map["floorMaxCMY"] = job.floorMaxCMY;
	map["floorRangeK"] = job.floorRangeK;
	map["floorMaxK"] = job.floorMaxK;
	map["enableDotSwap"] = job.enableDotSwap;
    map["createdAt"] = job.createdAt;
    
    return map;
}


// Update a print job from a QVariantMap
void PrintJobModel::updateJob(int index, const QVariantMap &jobData) {
    if (index < 0 || index >= m_jobs.size()) return;
    PrintJob &job = m_jobs[index];
    job.name = jobData["name"].toString();
    job.imagePath = jobData["imagePath"].toString();
    job.paperSize = jobData["paperSize"].toSize();
    job.resolution = jobData["resolution"].toSize();
    job.offset = jobData["offset"].toPoint();
    job.whiteStrategy = jobData["whiteStrategy"].toString();
    job.varnishType = jobData["varnishType"].toString();
    job.colorProfile = jobData["colorProfile"].toString();
    job.minInkThreshold = jobData["minInkThreshold"].toInt();
	job.smallDotThreshold = jobData["smallDotThreshold"].toInt();
	job.medDotThreshold = jobData["medDotThreshold"].toInt();
	job.enablePromotion = jobData["enablePromotion"].toBool();
	job.floorRangeCMY = static_cast<uint8_t>(jobData.value("floorRangeCMY", job.floorRangeCMY).toInt());
	job.floorMaxCMY = static_cast<uint8_t>(jobData.value("floorMaxCMY", job.floorMaxCMY).toInt());
	job.floorRangeK = static_cast<uint8_t>(jobData.value("floorRangeK", job.floorRangeK).toInt());
	job.floorMaxK = static_cast<uint8_t>(jobData.value("floorMaxK", job.floorMaxK).toInt());
	job.enableDotSwap = jobData.value("enableDotSwap", job.enableDotSwap).toBool();

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

    QByteArray jsonData = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(jsonData);
    QList<PrintJob> newJobs;

    // Parse a single PrintJob from JSON
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
        job.minInkThreshold = obj["minInkThreshold"].toInt();
		job.smallDotThreshold = obj["smallDotThreshold"].toInt();
		job.medDotThreshold = obj["medDotThreshold"].toInt();
		job.enablePromotion = obj["enablePromotion"].toBool();
		job.floorRangeCMY = static_cast<uint8_t>(obj.value("floorRangeCMY").toInt(24));
		job.floorMaxCMY = static_cast<uint8_t>(obj.value("floorMaxCMY").toInt(2));
		job.floorRangeK = static_cast<uint8_t>(obj.value("floorRangeK").toInt(12));
		job.floorMaxK = static_cast<uint8_t>(obj.value("floorMaxK").toInt(0));
		job.enableDotSwap = obj.value("enableDotSwap").toBool(false);
        job.createdAt = QDateTime::fromString(obj["createdAt"].toString(), Qt::ISODate);

        // If image data is embedded, reconstruct the image file
        if (obj.contains("imageData")) {
            QByteArray imageData = QByteArray::fromBase64(obj["imageData"].toString().toUtf8());

            QString originalExt = QFileInfo(job.imagePath).suffix();
            if (originalExt.isEmpty())
                originalExt = "png";

            QString newPath = QFileInfo(localPath).absoluteDir().filePath(job.id + "." + originalExt);

            QFile outImage(newPath);
            if (outImage.open(QIODevice::WriteOnly)) {
                outImage.write(imageData);
                outImage.close();
                job.imagePath = QUrl::fromLocalFile(newPath).toString();
            }
        }

        newJobs.append(job);
    };

    // Handle both array and single object formats
    if (doc.isArray()) {
        for (const QJsonValue &val : doc.array()) {
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

    beginInsertRows(QModelIndex(), m_jobs.size(), m_jobs.size() + newJobs.size() - 1);
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
        obj["minInkThreshold"] = job.minInkThreshold;
		obj["smallDotThreshold"] = job.smallDotThreshold;
		obj["medDotThreshold"] = job.medDotThreshold;
		obj["enablePromotion"] = job.enablePromotion;
		obj["floorRangeCMY"] = static_cast<int>(job.floorRangeCMY);
		obj["floorMaxCMY"] = static_cast<int>(job.floorMaxCMY);
		obj["floorRangeK"] = static_cast<int>(job.floorRangeK);
		obj["floorMaxK"] = static_cast<int>(job.floorMaxK);
		obj["enableDotSwap"] = job.enableDotSwap;
        obj["createdAt"] = job.createdAt.toString(Qt::ISODate);

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
