#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QImageReader>
#include <QPalette>
#include <QStyleFactory>

#include "PrintJobModel.h"
#include "ImageLoader.h"
#include "PrintJobOutput.h"
#include "PrintJobNocai.h"
#include "ImageEditor.h"
#include "ColorProfile.h"


/* Entry point for the RIP application.
 * - Sets a consistent Fusion style with a dark palette.
 * - Creates backend singletons and exposes them to QML.
 * - Sets an image allocation cap to avoid runaway memory use.
 * - Loads the main QML and starts the event loop.
 */
int main(int argc, char *argv[]) {

    QApplication app(argc, argv);
    
    // Force Fusion style
    QApplication::setStyle(QStyleFactory::create("Fusion"));	// Consistent cross‑platform look.

    // Dark palette (applies to widgets and influences Qt Quick Controls).
    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::WindowText, Qt::white);
    darkPalette.setColor(QPalette::Base, QColor(42, 42, 42));
    darkPalette.setColor(QPalette::AlternateBase, QColor(66, 66, 66));
    darkPalette.setColor(QPalette::ToolTipBase, Qt::white);
    darkPalette.setColor(QPalette::ToolTipText, Qt::white);
    darkPalette.setColor(QPalette::Text, Qt::white);
    darkPalette.setColor(QPalette::Button, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::ButtonText, Qt::white);
    darkPalette.setColor(QPalette::BrightText, Qt::red);
    darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));
    darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
    darkPalette.setColor(QPalette::HighlightedText, Qt::black);

    app.setPalette(darkPalette);
    
    QQmlApplicationEngine engine;
    
    // Backend components (owned by main; lifetime = entire app).
    PrintJobModel jobModel;
    ImageLoader imageLoader;
    ImageEditor imageEditor;
    PrintJobOutput printJobOutput;
    PrintJobNocai printJobNocaiOutput;
    ColorProfile colorProfile;

    // Expose C++ objects to QML (context properties for convenient access).
    engine.rootContext()->setContextProperty("jobModel", &jobModel);
    engine.rootContext()->setContextProperty("imageLoader", &imageLoader);
    engine.rootContext()->setContextProperty("imageEditor", &imageEditor);
    engine.rootContext()->setContextProperty("printJobOutput", &printJobOutput);
    engine.rootContext()->setContextProperty("printJobNocai", &printJobNocaiOutput);
    engine.rootContext()->setContextProperty("colorProfile", &colorProfile);
    
    // Cap decode allocations to reduce OOM risk with very large images (MB).
    // Set to 0 to disable the guard (not recommended).
    QImageReader::setAllocationLimit(1024);

    // Load QML UI and verify a root object was created.
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}
