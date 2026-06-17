#pragma once

#include <QObject>
#include <QString>

class PlatformCapabilities : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString platformName READ platformName CONSTANT)
    Q_PROPERTY(bool supportsCupsPrinting READ supportsCupsPrinting CONSTANT)
    Q_PROPERTY(bool supportsRipProcessing READ supportsRipProcessing CONSTANT)
    Q_PROPERTY(bool supportsDirectPrint READ supportsDirectPrint CONSTANT)

public:
    explicit PlatformCapabilities(QObject* parent = nullptr);

    QString platformName() const;
    bool supportsCupsPrinting() const;
    bool supportsRipProcessing() const;
    bool supportsDirectPrint() const;
};
