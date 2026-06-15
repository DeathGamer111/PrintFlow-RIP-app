import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.Material
import "."


// Top-level application window and navigation host.
ApplicationWindow {
    id: root
    visible: true
    width: 475		// Default App window width; keep modest to fit small screens.
    height: 615		// Default App window height.
    title: qsTr("Nocai RIP App")
    
    readonly property Theme theme: Theme { dark: true }

    // Optional: set window background to match theme
    color: theme.bg
    Material.theme: theme.dark ? Material.Dark : Material.Light
	Material.accent: theme.accent
	Material.background: theme.bg
	Material.foreground: theme.text

    // Lightweight app-scoped state shared across views.
    QtObject {
        id: appState
        property string selectedPrinter: "X-36NC (Photo Printer)"	// Current printer selection (name or ID).
        property string selectedPPD: ""								// Chosen PPD/profile path or identifier.
        property bool usingSimulatedPrinter: true					// When true, use mock device behavior.
		property bool usingMultiInkPrinter: true					// When true, use the PrintJobMultiInk backend.
		property int multiInkInkMode: 10							// 4,5,6,7,8,10 – current ink layout.
        property bool isGeneratingPRN: false						// Global flag to gate UI during PRN generation.
    }

    // Simple view stack; JobListView is our entry screen.
    StackView {
        id: stackView
        anchors.fill: parent

		// Push initial view and pass shared objects it needs.
        Component.onCompleted: {
        	// Startup defaults so the app is ready to print immediately
        	colorManager.selectedPrinter = appState.selectedPrinter
        	
			printJobMultiInk.setInkMode(appState.multiInkInkMode)
		    printJobMultiInk.enableDefaultInputCMYK(true)
		    printJobMultiInk.prepareAssets()
        
            stackView.push("qrc:/qml/JobListView.qml", {
                stackView: stackView,					// Allow child to navigate (push/pop)
                appState: appState,						// Share global app state
                jobModel: jobModel,						// Exposed C++ model (set in main.cppcontext)
                theme: root.theme
            })
        }
    }
}
