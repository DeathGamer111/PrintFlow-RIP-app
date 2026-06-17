#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QImageReader>
#include <QIcon>
#include <QPalette>
#include <QStyleFactory>

#include "PrintJobModel.h"
#include "ImageLoader.h"
#include "PrintJobOutput.h"
#include "PrintJobNocai.h"
#include "PrintJobMultiInk.h"
#include "NocaiDirectPrintClient.h"
#include "ImageEditor.h"
#include "ColorProfile.h"
#include "ColorManagementManager.h"

#include <QQuickStyle>
#include <QQuickWindow>


/* Entry point for the RIP application.
 * - Sets a consistent Fusion style with a dark palette.
 * - Creates backend singletons and exposes them to QML.
 * - Sets an image allocation cap to avoid runaway memory use.
 * - Loads the main QML and starts the event loop.
 */
int main(int argc, char *argv[]) {

    QApplication app(argc, argv);
    app.setDesktopFileName(QStringLiteral("RIP_App_Demo"));
    app.setWindowIcon(QIcon(QStringLiteral(":/assets/logo.png")));
  
    // Optional: set Material theme + accent via env vars
    qputenv("QT_QUICK_CONTROLS_MATERIAL_PRIMARY", "#14181F"); // charcoal
    qputenv("QT_QUICK_CONTROLS_MATERIAL_ACCENT",  "#2DD4BF"); // teal
    QQuickStyle::setStyle("Material");
    
    QQmlApplicationEngine engine;
    
    // Backend components (owned by main; lifetime = entire app).
    PrintJobModel jobModel;
    ImageLoader imageLoader;
    ImageEditor imageEditor;
    PrintJobOutput printJobOutput;
    PrintJobNocai printJobNocaiOutput;
    NocaiDirectPrintClient nocaiDirectPrint;
    PrintJobMultiInk printJobMultiInk;
    ColorProfile colorProfile;
    ColorManagementManager colorManager;

    // Expose C++ objects to QML (context properties for convenient access).
    engine.rootContext()->setContextProperty("jobModel", &jobModel);
    engine.rootContext()->setContextProperty("imageLoader", &imageLoader);
    engine.rootContext()->setContextProperty("imageEditor", &imageEditor);
    engine.rootContext()->setContextProperty("printJobOutput", &printJobOutput);
    engine.rootContext()->setContextProperty("printJobNocai", &printJobNocaiOutput);
    engine.rootContext()->setContextProperty("nocaiDirectPrint", &nocaiDirectPrint);
    engine.rootContext()->setContextProperty("printJobMultiInk", &printJobMultiInk);
    engine.rootContext()->setContextProperty("colorProfile", &colorProfile);
    engine.rootContext()->setContextProperty("colorManager", &colorManager);
    
    // load persisted settings early
    colorManager.load();

    // bind shared ColorManager to both backends
    printJobMultiInk.setColorManager(&colorManager);
    printJobMultiInk.setDirectPrintClient(&nocaiDirectPrint);
    printJobNocaiOutput.setColorManager(&colorManager);
    
    // Cap decode allocations to reduce OOM risk with very large images (MB).
    // Set to 0 to disable the guard (not recommended).
    QImageReader::setAllocationLimit(1024);

    // Load QML UI and verify a root object was created.
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}
