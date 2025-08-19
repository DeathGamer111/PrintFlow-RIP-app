import QtQuick
import QtQuick.Layouts
import QtQuick.Controls


// Top-level application window and navigation host.
ApplicationWindow {
    id: root
    visible: true
    width: 450		// Default App window width; keep modest to fit small screens.
    height: 600		// Default App window height.
    title: qsTr("RIP Printer App")
    
    // Lightweight app-scoped state shared across views.
    QtObject {
        id: appState
        property string selectedPrinter: ""				// Current printer selection (name or ID).
        property string selectedPPD: ""					// Chosen PPD/profile path or identifier.
        property bool usingSimulatedPrinter: false		// When true, use mock device behavior.
        property bool isGeneratingPRN: false			// Global flag to gate UI during PRN generation.
    }

    // Simple view stack; JobListView is our entry screen.
    StackView {
        id: stackView
        anchors.fill: parent

		// Push initial view and pass shared objects it needs.
        Component.onCompleted: {
            stackView.push("qrc:/qml/JobListView.qml", {
                stackView: stackView,					// Allow child to navigate (push/pop).
                appState: appState,						// Share global app state.
                jobModel: jobModel						// Exposed C++ model (set in main.cpp context).
            })
        }
    }
}
