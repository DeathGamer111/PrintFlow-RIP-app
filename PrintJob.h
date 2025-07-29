// PrintJob.h
#include <QString>
#include <QPoint>
#include <QDateTime>
#include <QSize>

/*******************************************************************
    PrintJob struct encapsulates all relevant data for a print task.
    Used as a model for job management, configuration, and output.
********************************************************************/

struct PrintJob {
    QString id;                 // Unique identifier for the job
    QString name;               // Display name
    QString imagePath;          // Path to the input image

    QSize paperSize;            // Paper size in pixels (or user-defined)
    QSize resolution;           // Output resolution (DPI)
    QPoint offset;              // Position offset on page

    QString whiteStrategy;      // Strategy for white ink printing
    QString varnishType;        // Type of varnish applied
    QString colorProfile;       // ICC color profile or label
    
	int minInkThreshold;		// Minimum Ink Threshold Value
	int smallDotThreshold;		// Threshold for Small Ink Dots
	int medDotThreshold;		// Threshold for Medium Ink Dots
	bool enablePromotion;		// Dot Promotion Enabled/Disabled

    QDateTime createdAt;        // Timestamp when job was created
};

