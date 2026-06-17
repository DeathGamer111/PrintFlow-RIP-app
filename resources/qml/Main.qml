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
    title: qsTr("PrintFlow")
    
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
        property string platformName: platformCapabilities.platformName
        property bool supportsCupsPrinting: platformCapabilities.supportsCupsPrinting
        property bool supportsRipProcessing: platformCapabilities.supportsRipProcessing
        property bool supportsDirectPrint: platformCapabilities.supportsDirectPrint
        property string multiInkOutputMode: colorManager.multiInkOutputMode
        property int sdkSelectedPrinterIndex: colorManager.directPrintSetting("selectedPrinterIndex")
        property int sdkPrintDirection: colorManager.directPrintSetting("printDirection")
        property int sdkPrintSpeed: colorManager.directPrintSetting("printSpeed")
        property int sdkWcSequence: colorManager.directPrintSetting("wcSequence")
        property int sdkEclosionGrade: colorManager.directPrintSetting("eclosionGrade")
        property int sdkHeadSelect: colorManager.directPrintSetting("headSelect")
        property int sdkWhiteInkPercent: colorManager.directPrintSetting("whiteInkPercent")
        property int sdkWhiteInkPassCount: colorManager.directPrintSetting("whiteInkPassCount")
        property int sdkVarnishInkPercent: colorManager.directPrintSetting("varnishInkPercent")
        property int sdkVarnishInkPassCount: colorManager.directPrintSetting("varnishInkPassCount")
        property int sdkHeadVoltage: colorManager.directPrintSetting("headVoltage")
        property int sdkDisableUv0: colorManager.directPrintSetting("disableUv0")
        property int sdkDisableUv1: colorManager.directPrintSetting("disableUv1")
        property int sdkDisableUv2: colorManager.directPrintSetting("disableUv2")
        property int sdkDisableUv3: colorManager.directPrintSetting("disableUv3")
        property int sdkDisableUv4: colorManager.directPrintSetting("disableUv4")
        property int sdkDisableUv5: colorManager.directPrintSetting("disableUv5")
        property int sdkCarReset: colorManager.directPrintSetting("carReset")
        property int sdkStripBlank: colorManager.directPrintSetting("stripBlank")
        property int sdkBlankDistance: colorManager.directPrintSetting("blankDistance")
        property int sdkPass: colorManager.directPrintSetting("pass")
        property int sdkVsdMode: colorManager.directPrintSetting("vsdMode")
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
            nocaiDirectPrint.sdkRootPath = colorManager.directPrintSdkRootPath
        	
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
