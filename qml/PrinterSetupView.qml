import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.platform
import "."

Page {
    id: root
    required property StackView stackView
    required property var appState
    required property Theme theme

    background: Rectangle { color: theme.bg }

    // Tracks which ICC dropdown to update after a file is chosen.
    property string iccDialogTarget: "output" // "output" | "inputCMYK" | "deviceLink"
    property string linearizationDialogTarget: "printerLinearization"
    property string sdkConnectionState: "Not connected"
    property string sdkSelectedPrinterName: ""
    property var sdkPrinterStatusInfo: ({})
    property var sdkPrinterFirmwareInfo: ({})

    // In-memory ICC list for dropdowns; populated from backend and user uploads.
    ListModel { id: iccProfileModel }
    ListModel { id: deviceLinkModel }
    ListModel { id: sdkPrinterModel }
    
    property bool _syncingTabs: false

    // Static capability map used for the simulated Nocai devices.
    property var nocaiPrinterCapabilities: {
        "X-33": {
            resolutions: ["720x720", "720x1440", "720x2160"],
            mediaSizes: ["A1", "A2", "A3", "A4", "A5", "A6", "Tabloid"],
            duplexModes: ["None"],
            colorModes: ["CMYK", "CMYKWW", "CMYKWV"]
        },

        "X-24": {
            resolutions: ["720x720", "720x1440", "720x2160"],
            mediaSizes: ["A2", "A3", "A4", "A5", "A6", "Tabloid"],
            duplexModes: ["None"],
            colorModes: ["CMYK", "CMYKWW", "CMYKWV"]
        },

        "X-36NC (Photo Printer)": {
            resolutions: ["720x720", "720x1440", "720x2160"],
            mediaSizes: ["A2", "A3", "A4", "A5", "A6", "Tabloid"],
            duplexModes: ["None"],
            colorModes: [
                "4: CMYK",
                "5: CMYK+W",
                "6: CMYK+Lc+Lm",
                "7: CMYK+Lc+Lm+W",
                "8: CMYK+Lc+Lm+Lk+LLk",
                "10: CMYK+Lc+Lm+Lk+LLk+W+V"
            ]
        }
    }

    function hasPrinterSelected() {
        return appState.selectedPrinter && appState.selectedPrinter.length > 0
    }

    function activeBackend() {
        if (!hasPrinterSelected()) return null
        return appState.usingMultiInkPrinter ? printJobMultiInk : printJobNocai
    }

    function normalizePath(urlOrPath) {
        const s = (urlOrPath || "").toString()
        return s.startsWith("file://") ? s.slice(7) : s
    }

    function isX36MultiInk() {
        return appState.usingSimulatedPrinter
               && appState.usingMultiInkPrinter
               && appState.selectedPrinter === "X-36NC (Photo Printer)"
    }

    function setDirectSetting(key, value) {
        colorManager.setDirectPrintSetting(key, value)
    }

    function refreshSdkPrinters() {
        sdkPrinterModel.clear()
        nocaiDirectPrint.sdkRootPath = colorManager.directPrintSdkRootPath
        const ok = nocaiDirectPrint.refreshPrinters()
        const printers = nocaiDirectPrint.printers
        for (let i = 0; i < printers.length; ++i)
            sdkPrinterModel.append(printers[i])

        syncSdkPrinterCombo()
        toast.show(ok ? "SDK printer list refreshed." : "SDK unavailable: " + nocaiDirectPrint.lastError)
    }

    function connectSdkPrinter() {
        if (appState.sdkSelectedPrinterIndex >= 0)
            nocaiDirectPrint.choosePrinter(appState.sdkSelectedPrinterIndex)

        const ok = nocaiDirectPrint.connectPrinter()
        sdkConnectionState = ok ? "Connected" : "Connection failed"
        toast.show(ok ? "SDK printer connected." : "ConnectPrinter failed: " + nocaiDirectPrint.lastError)
        if (ok)
            refreshSdkStatusAndInfo()
    }

    function refreshSdkStatusAndInfo() {
        if (appState.sdkSelectedPrinterIndex >= 0)
            nocaiDirectPrint.choosePrinter(appState.sdkSelectedPrinterIndex)

        const status = nocaiDirectPrint.getPrinterStatus()
        sdkPrinterStatusInfo = status
        const info = nocaiDirectPrint.getPrinterInfo()
        sdkPrinterFirmwareInfo = info

        if (status.ok || info.ok)
            sdkConnectionState = "Connected"
        else if (nocaiDirectPrint.lastError && nocaiDirectPrint.lastError.length > 0)
            sdkConnectionState = "Unavailable"
    }

    function syncSdkPrinterCombo() {
        sdkPrinterCombo.currentIndex = -1
        sdkSelectedPrinterName = ""
        for (let i = 0; i < sdkPrinterModel.count; ++i) {
            if (sdkPrinterModel.get(i).index === appState.sdkSelectedPrinterIndex) {
                sdkPrinterCombo.currentIndex = i
                sdkSelectedPrinterName = sdkPrinterModel.get(i).name
                break
            }
        }
    }

	function currentOutputProfileInkMode() {
		if (appState.usingMultiInkPrinter) {
		    return appState.multiInkInkMode || 4
		}

		// Non-multi-ink Nocai path:
		// treat as Family A fallback for now
		return 4
	}

	function resolvedOutputProfileForCurrentSelection() {
		if (!hasPrinterSelected())
		    return ""

		const inkMode = currentOutputProfileInkMode()
		return colorManager.effectiveOutputProfileForPrinterAndInkMode(appState.selectedPrinter, inkMode)
	}

	function applyResolvedOutputProfileToBackend() {
		const backend = activeBackend()
		if (!backend)
		    return

		const resolved = resolvedOutputProfileForCurrentSelection()
		if (resolved && resolved.length > 0) {
		    backend.setDefaultOutputICCProfile(resolved)
		}
	}
	
	function currentFamilyKey() {
		return colorManager.outputProfileFamilyForInkMode(currentOutputProfileInkMode())
	}

	function resolvedLinearizationForCurrentSelection() {
		if (!hasPrinterSelected())
		    return ""

		const inkMode = currentOutputProfileInkMode()
		return colorManager.effectiveLinearizationPathForPrinterAndInkMode(appState.selectedPrinter, inkMode)
	}

	function syncUIFromAppState() {

		// --- Simulated / Nocai path ---
		if (appState.usingSimulatedPrinter) {
		    const selected = appState.selectedPrinter || ""

		    // Pre-select Nocai model if one is already chosen
		    const nocaiNames = ["X-33", "X-36NC (Photo Printer)"]
		    const nocaiIndex = nocaiNames.indexOf(selected)
		    if (nocaiIndex >= 0)
		        nocaiPrinterComboBox.currentIndex = nocaiIndex

		    const isMultiInk = (selected === "X-36NC (Photo Printer)")
		    appState.usingMultiInkPrinter = isMultiInk

		    // Choose backend and ensure assets/ICC are ready
		    let backend
		    if (isMultiInk) {
		        backend = printJobMultiInk
		        printJobMultiInk.prepareAssets()
		    } else {
		        backend = printJobNocai
		        printJobNocai.prepareNocaiAssets()
		    }

		    // Rebuild ICC list from backend
		    iccProfileModel.clear()
		    const profiles = backend.getAvailableICCProfiles()
		    for (let i = 0; i < profiles.length; ++i) {
		        iccProfileModel.append(profiles[i])
		    }

		    // Sync Output ICC dropdown
			const resolvedOutput = root.resolvedOutputProfileForCurrentSelection()
			if (resolvedOutput && resolvedOutput.length > 0) {
				backend.setDefaultOutputICCProfile(resolvedOutput)
			}

			const currentDefault = (resolvedOutput && resolvedOutput.length > 0)
					? resolvedOutput
					: backend.getDefaultOutputICCProfile()

			iccProfileDropdown.currentIndex = -1
			for (let i = 0; i < iccProfileModel.count; ++i) {
				if (iccProfileModel.get(i).path === currentDefault) {
					iccProfileDropdown.currentIndex = i
					break
				}
			}

		    // Sync Input CMYK dropdown
		    const currentInputCmyk = backend.getDefaultInputCMYKProfile()
		    inputCmykDropdown.currentIndex = -1
		    for (let i = 0; i < iccProfileModel.count; ++i) {
		        if (iccProfileModel.get(i).path === currentInputCmyk) {
		            inputCmykDropdown.currentIndex = i
		            break
		        }
		    }

		    // Sync toggle
		    useInputCmykSwitch.checked = backend.checkDefaultInputCMYK()

		    // Sync Ink Layout for multi-ink printer
		    if (isMultiInk) {
		        for (let i = 0; i < inkLayoutCombo.model.count; ++i) {
		            const elem = inkLayoutCombo.model.get(i)
		            if (elem.value === appState.multiInkInkMode) {
		                inkLayoutCombo.currentIndex = i
		                break
		            }
		        }
		        printJobMultiInk.setInkMode(appState.multiInkInkMode)
		        printJobMultiInk.enableDefaultInputCMYK(true)
		    }

		    // Guard in case the DeviceLink UI hasn't been instantiated yet
		    if (typeof deviceLinkModel !== "undefined" &&
		        typeof deviceLinkSwitch !== "undefined" &&
		        typeof deviceLinkDropdown !== "undefined") {

		        deviceLinkModel.clear()

		        if (isMultiInk) {
		            const links = printJobMultiInk.getAvailableDeviceLinkProfiles()
		            for (let i = 0; i < links.length; ++i) {
		                deviceLinkModel.append(links[i])
		            }

		            // Toggle from backend
		            deviceLinkSwitch.checked = printJobMultiInk.isDeviceLinkEnabled()

		            // Sync dropdown selection from backend default
		            const currentDL = printJobMultiInk.getDefaultDeviceLinkProfile()
		            deviceLinkDropdown.currentIndex = -1
		            for (let i = 0; i < deviceLinkModel.count; ++i) {
		                if (deviceLinkModel.get(i).path === currentDL) {
		                    deviceLinkDropdown.currentIndex = i
		                    break
		                }
		            }
		        } else {
		            // Not multi-ink -> ensure it's off/empty
		            deviceLinkSwitch.checked = false
		            deviceLinkDropdown.currentIndex = -1
		        }
		    }

		// --- Network printer path ---
		} else if (hasPrinterSelected()) {
		    const printers = printJobOutput.detectedPrinters
		    if (printers && printers.length) {
		        for (let i = 0; i < printers.length; ++i) {
		            if (printers[i] === appState.selectedPrinter) {
		                printerComboBox.currentIndex = i
		                break
		            }
		        }
		    }

		    // OPTIONAL: if DeviceLink controls exist, hard-disable them on network tab
		    if (typeof deviceLinkSwitch !== "undefined") {
		        deviceLinkSwitch.checked = false
		    }
		    if (typeof deviceLinkDropdown !== "undefined") {
		        deviceLinkDropdown.currentIndex = -1
		    }
		    if (typeof deviceLinkModel !== "undefined") {
		        deviceLinkModel.clear()
		    }
		}

        if (typeof sdkPrinterCombo !== "undefined")
            syncSdkPrinterCombo()
	}

    function doSave() {
        if (!hasPrinterSelected()) return
        toast.show("Printer setup complete: " + appState.selectedPrinter)
        stackView.pop()
    }

	Component.onCompleted: {
		printJobOutput.refreshDetectedPrinters()

		// App defaults: X-36NC Photo Printer + 10-color MultiInk
		if (!appState.selectedPrinter || appState.selectedPrinter.length === 0) {
		    appState.selectedPrinter = "X-36NC (Photo Printer)"
		    appState.usingSimulatedPrinter = true
		    appState.usingMultiInkPrinter = true
		    appState.multiInkInkMode = 10

		    printJobMultiInk.setInkMode(10)
		    printJobMultiInk.enableDefaultInputCMYK(true)
		}

		_syncingTabs = true
		printerTabs.currentIndex = (appState.usingSimulatedPrinter ? 0 : 1)
		_syncingTabs = false

		Qt.callLater(syncUIFromAppState)
	}

    onVisibleChanged: {
		if (visible) {
		    _syncingTabs = true
		    printerTabs.currentIndex = (appState.usingSimulatedPrinter ? 0 : 1)
		    _syncingTabs = false

		    Qt.callLater(syncUIFromAppState)
		}
    }

    Rectangle {
        id: headerBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 60
        color: theme.surface

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            spacing: 10

            ThemedButton {
                text: "Back"
                theme: root.theme
                Layout.preferredWidth: 88
                padding: 12
                font.pixelSize: 15
                onClicked: root.stackView.pop()
            }

            Item { Layout.fillWidth: true }

            Label {
                text: "Printer Setup"
                color: theme.text
                font.pixelSize: 20
                font.weight: Font.Medium
                horizontalAlignment: Text.AlignHCenter
                Layout.alignment: Qt.AlignVCenter
            }

            Item { Layout.fillWidth: true }

            ThemedButton {
                text: "Save"
                theme: root.theme
                Layout.preferredWidth: 88
                padding: 12
                font.pixelSize: 15
                enabled: hasPrinterSelected()
                onClicked: root.doSave()
            }
        }
    }

    ScrollView {
        id: scroll
        anchors.top: headerBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 12
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: scroll.availableWidth
            spacing: 14
            Layout.alignment: Qt.AlignHCenter

            // =========================
            // Printer Mode
            // =========================
            Pane {
                Layout.fillWidth: true
                padding: 12

                background: Rectangle {
                    color: theme.surface
                    radius: 12
                    border.width: 1
                    border.color: theme.divider
                }

                ColumnLayout {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: Math.min(parent.width, 520)
                    spacing: 12

                    Label {
                        text: "Printer Mode"
                        color: theme.text
                        font.pixelSize: 18
                        font.weight: Font.Medium
                        Layout.alignment: Qt.AlignHCenter
                    }

                    Rectangle { height: 1; Layout.fillWidth: true; color: theme.divider; opacity: 0.8 }

                    TabBar {
                        id: printerTabs
                        Layout.fillWidth: true
                        TabButton { text: "Nocai Printer" }
                        TabButton { text: "Network Printer" }

                        onCurrentIndexChanged: {
                            if (root._syncingTabs) return

                            if (currentIndex === 0) {
                                appState.usingSimulatedPrinter = true
                            } else if (currentIndex === 1) {
                                appState.usingSimulatedPrinter = false
                                appState.usingMultiInkPrinter = false
                            }
                            Qt.callLater(root.syncUIFromAppState)
                        }
                    }

                    StackLayout {
                        id: tabStack
                        currentIndex: printerTabs.currentIndex
                        Layout.fillWidth: true

                        // --- Tab 0: Nocai (simulated) printers ---
                        ColumnLayout {
                            spacing: 10
                            Layout.fillWidth: true

                            Label {
                                text: "Select your Nocai printer"
                                color: theme.text
                                font.weight: Font.Medium
                                Layout.alignment: Qt.AlignHCenter
                            }

                            ComboBox {
                                id: nocaiPrinterComboBox
                                Layout.fillWidth: true
                                model: [
                                    "X-33",
                                    "X-36NC (Photo Printer)"
                                ]

                                onActivated: {
                                    const selected = currentText
                                    if (!selected || selected.length <= 0) return

                                    appState.selectedPrinter = selected
                                    appState.usingSimulatedPrinter = true

                                    const isMultiInk = (selected === "X-36NC (Photo Printer)")
                                    appState.usingMultiInkPrinter = isMultiInk

									if (isMultiInk) {
										const validModes = [4, 5, 6, 7, 8, 10]
										if (validModes.indexOf(appState.multiInkInkMode) === -1) {
											appState.multiInkInkMode = 10
										}

										for (let i = 0; i < inkLayoutCombo.model.count; ++i) {
											const elem = inkLayoutCombo.model.get(i)
											if (elem.value === appState.multiInkInkMode) {
												inkLayoutCombo.currentIndex = i
												break
											}
										}

										printJobMultiInk.setInkMode(appState.multiInkInkMode)
										printJobMultiInk.enableDefaultInputCMYK(true)
									}

                                    let backend
									if (isMultiInk) {
										backend = printJobMultiInk
										printJobMultiInk.prepareAssets()
									} else {
										backend = printJobNocai
										printJobNocai.prepareNocaiAssets()
									}

									iccProfileModel.clear()
									const profiles = backend.getAvailableICCProfiles()
									for (let i = 0; i < profiles.length; ++i)
										iccProfileModel.append(profiles[i])

									Qt.callLater(root.syncUIFromAppState)

									toast.show((isMultiInk ? "Multi-ink " : "") + "Nocai printer selected: " + selected)
                                }
                            }

                            Label {
                                text: "Note: The Nocai engine generates PRN files only."
                                color: theme.subtext
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                                horizontalAlignment: Text.AlignHCenter
                            }

                            // Ink layout selection – only for X-36NC MultiInk printer
                            ColumnLayout {
                                visible: appState.usingSimulatedPrinter
                                         && appState.selectedPrinter === "X-36NC (Photo Printer)"
                                Layout.fillWidth: true
                                spacing: 8

                                Label { text: "Ink Layout"; color: theme.text; font.bold: true }

                                ComboBox {
                                    id: inkLayoutCombo
                                    Layout.fillWidth: true

                                    model: ListModel {
                                        ListElement { label: "4 – CMYK";                    value: 4  }
                                        ListElement { label: "5 – CMYK+W";                  value: 5  }
                                        ListElement { label: "6 – CMYK+Lc+Lm";              value: 6  }
                                        ListElement { label: "7 – CMYK+Lc+Lm+W";            value: 7  }
                                        ListElement { label: "8 – CMYK+Lc+Lm+Lk+LLk";       value: 8  }
                                        ListElement { label: "10 – CMYK+Lc+Lm+Lk+LLk+W+V";  value: 10 }
                                    }
                                    textRole: "label"

									onActivated: {
										const elem = model.get(currentIndex)
										const mode = elem.value
										appState.multiInkInkMode = mode
										printJobMultiInk.setInkMode(mode)

										root.applyResolvedOutputProfileToBackend()
										Qt.callLater(root.syncUIFromAppState)

										toast.show("Multi-ink layout set to " + elem.label)
									}
                                }
                            }

	                            ColumnLayout {
	                                visible: root.isX36MultiInk()
	                                Layout.fillWidth: true
	                                spacing: 10

	                                Rectangle { height: 1; Layout.fillWidth: true; color: theme.divider; opacity: 0.8 }

	                                Label {
	                                    text: "Direct Print SDK"
	                                    color: theme.text
	                                    font.bold: true
	                                    Layout.alignment: Qt.AlignHCenter
	                                }

	                                Label {
	                                    text: "Output Mode"
	                                    color: theme.text
	                                    font.bold: true
	                                    Layout.alignment: Qt.AlignHCenter
	                                }

	                                ComboBox {
	                                    id: directOutputModeCombo
	                                    Layout.fillWidth: true
                                    model: ListModel {
                                        ListElement { label: "PRN Generation"; value: "prn" }
                                        ListElement { label: "Direct to Print"; value: "direct" }
                                    }
                                    textRole: "label"
                                    currentIndex: appState.multiInkOutputMode === "direct" ? 1 : 0

                                    onActivated: {
                                        const selected = model.get(currentIndex)
                                        appState.multiInkOutputMode = selected.value
                                        colorManager.setMultiInkOutputMode(selected.value)
                                        toast.show("Output mode set to " + selected.label)
                                    }
                                }

                                Label {
                                    text: appState.multiInkOutputMode === "direct"
                                          ? "Direct mode streams the MultiInk raster to the Nocai SDK."
                                          : "PRN mode saves a file for testing and debugging."
	                                    color: theme.subtext
	                                    wrapMode: Text.WordWrap
	                                    Layout.fillWidth: true
	                                    horizontalAlignment: Text.AlignHCenter
	                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 10

	                                    ThemedButton {
	                                        text: "Refresh SDK Printers"
	                                        theme: root.theme
	                                        Layout.fillWidth: true
	                                        Layout.preferredHeight: 40
	                                        onClicked: root.refreshSdkPrinters()
	                                    }

	                                    Label {
	                                        text: nocaiDirectPrint.available ? "SDK ready" : "SDK unavailable"
	                                        color: nocaiDirectPrint.available ? theme.accent : theme.warning
	                                        Layout.alignment: Qt.AlignVCenter
	                                    }
	                                }

                                ComboBox {
                                    id: sdkPrinterCombo
                                    Layout.fillWidth: true
                                    model: sdkPrinterModel
                                    textRole: "name"
                                    displayText: currentIndex >= 0 ? currentText : "(no SDK printer selected)"

	                                    onActivated: {
	                                        if (sdkPrinterModel.count <= 0) return
	                                        const selected = sdkPrinterModel.get(currentIndex)
	                                        appState.sdkSelectedPrinterIndex = selected.index
	                                        root.sdkSelectedPrinterName = selected.name
	                                        root.setDirectSetting("selectedPrinterIndex", selected.index)
	                                        nocaiDirectPrint.choosePrinter(selected.index)
	                                    }
	                                }

	                                RowLayout {
	                                    Layout.fillWidth: true
	                                    spacing: 10

	                                    ThemedButton {
	                                        text: root.sdkConnectionState === "Connected" ? "Connected" : "Connect"
	                                        theme: root.theme
	                                        Layout.fillWidth: true
	                                        Layout.preferredHeight: 40
	                                        background: Rectangle {
	                                            radius: 6
	                                            color: root.sdkConnectionState === "Connected" ? "#1F8A5B" : "#A33A3A"
	                                            border.width: 1
	                                            border.color: root.theme.divider
	                                        }
	                                        onClicked: root.connectSdkPrinter()
	                                    }

	                                    ThemedButton {
	                                        text: "Refresh Status"
	                                        theme: root.theme
	                                        Layout.fillWidth: true
	                                        Layout.preferredHeight: 40
	                                        onClicked: root.refreshSdkStatusAndInfo()
	                                    }
	                                }

	                                Label {
	                                    Layout.fillWidth: true
	                                    text: "SDK Printer: " + (root.sdkSelectedPrinterName.length > 0 ? root.sdkSelectedPrinterName : "(none)") +
	                                              "\nConnection: " + root.sdkConnectionState +
	                                              "\n" + (nocaiDirectPrint.lastError && nocaiDirectPrint.lastError.length > 0
	                                                     ? nocaiDirectPrint.lastError
	                                                     : nocaiDirectPrint.statusText())
	                                    color: root.sdkConnectionState === "Connected" ? theme.subtext : theme.warning
	                                    wrapMode: Text.WordWrap
	                                    horizontalAlignment: Text.AlignHCenter
	                                }

                                GridLayout {
                                    Layout.fillWidth: true
                                    columns: 2
                                    columnSpacing: 12
                                    rowSpacing: 8

                                    Label { text: "Print Direction"; color: theme.text }
                                    SpinBox {
                                        from: 0; to: 3; value: appState.sdkPrintDirection
                                        onValueModified: { appState.sdkPrintDirection = value; root.setDirectSetting("printDirection", value) }
                                    }

                                    Label { text: "Print Speed"; color: theme.text }
                                    SpinBox {
                                        from: 0; to: 3; value: appState.sdkPrintSpeed
                                        onValueModified: { appState.sdkPrintSpeed = value; root.setDirectSetting("printSpeed", value) }
                                    }

                                    Label { text: "WC Sequence"; color: theme.text }
                                    SpinBox {
                                        from: 0; to: 1; value: appState.sdkWcSequence
                                        onValueModified: { appState.sdkWcSequence = value; root.setDirectSetting("wcSequence", value) }
                                    }

                                    Label { text: "Eclosion Grade"; color: theme.text }
                                    SpinBox {
                                        from: 0; to: 3; value: appState.sdkEclosionGrade
                                        onValueModified: { appState.sdkEclosionGrade = value; root.setDirectSetting("eclosionGrade", value) }
                                    }

                                    Label { text: "Head Select"; color: theme.text }
                                    SpinBox {
                                        from: 0; to: 2; value: appState.sdkHeadSelect
                                        onValueModified: { appState.sdkHeadSelect = value; root.setDirectSetting("headSelect", value) }
                                    }

                                    Label { text: "White Ink Percent"; color: theme.text }
                                    SpinBox {
                                        from: 0; to: 9; value: appState.sdkWhiteInkPercent
                                        onValueModified: { appState.sdkWhiteInkPercent = value; root.setDirectSetting("whiteInkPercent", value) }
                                    }

                                    Label { text: "White Ink Pass"; color: theme.text }
                                    SpinBox {
                                        from: 0; to: 255; value: appState.sdkWhiteInkPassCount
                                        onValueModified: { appState.sdkWhiteInkPassCount = value; root.setDirectSetting("whiteInkPassCount", value) }
                                    }

                                    Label { text: "Varnish Ink Percent"; color: theme.text }
                                    SpinBox {
                                        from: 0; to: 9; value: appState.sdkVarnishInkPercent
                                        onValueModified: { appState.sdkVarnishInkPercent = value; root.setDirectSetting("varnishInkPercent", value) }
                                    }

                                    Label { text: "Varnish Ink Pass"; color: theme.text }
                                    SpinBox {
                                        from: 0; to: 255; value: appState.sdkVarnishInkPassCount
                                        onValueModified: { appState.sdkVarnishInkPassCount = value; root.setDirectSetting("varnishInkPassCount", value) }
                                    }

                                    Label { text: "Head Voltage"; color: theme.text }
                                    SpinBox {
                                        from: 400; to: 600; value: appState.sdkHeadVoltage
                                        onValueModified: { appState.sdkHeadVoltage = value; root.setDirectSetting("headVoltage", value) }
                                    }

                                    Label { text: "Car Reset"; color: theme.text }
                                    SpinBox {
                                        from: 0; to: 1; value: appState.sdkCarReset
                                        onValueModified: { appState.sdkCarReset = value; root.setDirectSetting("carReset", value) }
                                    }

                                    Label { text: "Strip Blank"; color: theme.text }
                                    SpinBox {
                                        from: 0; to: 2; value: appState.sdkStripBlank
                                        onValueModified: { appState.sdkStripBlank = value; root.setDirectSetting("stripBlank", value) }
                                    }

                                    Label { text: "Blank Distance"; color: theme.text }
                                    SpinBox {
                                        from: 0; to: 65535; value: appState.sdkBlankDistance
                                        onValueModified: { appState.sdkBlankDistance = value; root.setDirectSetting("blankDistance", value) }
                                    }

                                    Label { text: "Print Pass"; color: theme.text }
                                    SpinBox {
                                        from: 0; to: 255; value: appState.sdkPass
                                        onValueModified: { appState.sdkPass = value; root.setDirectSetting("pass", value) }
                                    }

                                    Label { text: "VsdMode"; color: theme.text }
                                    SpinBox {
                                        from: 0; to: 65535; value: appState.sdkVsdMode
                                        onValueModified: { appState.sdkVsdMode = value; root.setDirectSetting("vsdMode", value) }
                                    }
                                }

	                                Label {
	                                    text: "Disable UV Lights"
	                                    color: theme.text
	                                    font.bold: true
	                                    Layout.alignment: Qt.AlignHCenter
	                                }

                                GridLayout {
                                    Layout.fillWidth: true
                                    columns: 2
                                    columnSpacing: 12
                                    rowSpacing: 6

                                    CheckBox {
                                        text: "R lamp R->L off"; checked: appState.sdkDisableUv0 === 1
                                        onToggled: { appState.sdkDisableUv0 = checked ? 1 : 0; root.setDirectSetting("disableUv0", appState.sdkDisableUv0) }
                                    }
                                    CheckBox {
                                        text: "R lamp L->R off"; checked: appState.sdkDisableUv1 === 1
                                        onToggled: { appState.sdkDisableUv1 = checked ? 1 : 0; root.setDirectSetting("disableUv1", appState.sdkDisableUv1) }
                                    }
                                    CheckBox {
                                        text: "L lamp R->L off"; checked: appState.sdkDisableUv2 === 1
                                        onToggled: { appState.sdkDisableUv2 = checked ? 1 : 0; root.setDirectSetting("disableUv2", appState.sdkDisableUv2) }
                                    }
                                    CheckBox {
                                        text: "L lamp L->R off"; checked: appState.sdkDisableUv3 === 1
                                        onToggled: { appState.sdkDisableUv3 = checked ? 1 : 0; root.setDirectSetting("disableUv3", appState.sdkDisableUv3) }
                                    }
                                    CheckBox {
                                        text: "UV lamp R->L off"; checked: appState.sdkDisableUv4 === 1
                                        onToggled: { appState.sdkDisableUv4 = checked ? 1 : 0; root.setDirectSetting("disableUv4", appState.sdkDisableUv4) }
                                    }
                                    CheckBox {
                                        text: "UV lamp L->R off"; checked: appState.sdkDisableUv5 === 1
                                        onToggled: { appState.sdkDisableUv5 = checked ? 1 : 0; root.setDirectSetting("disableUv5", appState.sdkDisableUv5) }
                                    }
                                }
                            }
                        }

                        // --- Tab 1: Network printers ---
                        ColumnLayout {
                            spacing: 10
                            Layout.fillWidth: true

                            Label {
                                text: "Select a network printer"
                                color: theme.text
                                font.weight: Font.Medium
                            }

                            ComboBox {
                                id: printerComboBox
                                Layout.fillWidth: true
                                model: printJobOutput.detectedPrinters

                                onActivated: {
                                    const name = currentText
                                    if (printJobOutput.loadPrinter(name)) {
                                        appState.selectedPrinter = name
                                        appState.usingSimulatedPrinter = false
                                        appState.usingMultiInkPrinter = false

                                        // Warm the backend capability lists
                                        printJobOutput.supportedResolutions()
                                        printJobOutput.supportedMediaSizes()
                                        printJobOutput.supportedDuplexModes()
                                        printJobOutput.supportedColorModes()

                                        toast.show("Network printer loaded: " + name)
                                    } else {
                                        toast.show("Failed to load printer: " + name)
                                    }
                                }
                            }

                            ThemedButton {
                                text: "Refresh List"
                                theme: root.theme
                                padding: 12
                                font.pixelSize: 15
                                onClicked: printJobOutput.refreshDetectedPrinters()
                            }
                        }

		                    }
		                }
		            }
            
            // DeviceLink controls (MultiInk only)
			Pane {
				Layout.fillWidth: true
				padding: 12
				visible: appState.usingSimulatedPrinter && appState.usingMultiInkPrinter

				background: Rectangle {
					color: theme.surface
					radius: 12
					border.width: 1
					border.color: theme.divider
				}

				ColumnLayout {
					anchors.horizontalCenter: parent.horizontalCenter
					width: Math.min(parent.width, 520)
					spacing: 10

					Label {
						text: "DeviceLink (Overrides ICC)"
						color: theme.text
						font.pixelSize: 16
						font.weight: Font.Medium
						Layout.alignment: Qt.AlignHCenter
					}

					Rectangle { height: 1; Layout.fillWidth: true; color: theme.divider; opacity: 0.8 }

					RowLayout {
						Layout.fillWidth: true
						spacing: 10

						Label { text: "Enable DeviceLink"; color: theme.text; Layout.fillWidth: true }

						Switch {
						    id: deviceLinkSwitch
						    checked: false
						    onToggled: {
						        printJobMultiInk.enableDeviceLink(checked)
						        toast.show(checked ? "DeviceLink enabled (ICC bypassed)" : "DeviceLink disabled (ICC active)")
						    }
						}
					}

					RowLayout {
						Layout.fillWidth: true
						spacing: 10
						enabled: deviceLinkSwitch.checked

						ComboBox {
						    id: deviceLinkDropdown
						    Layout.fillWidth: true
						    model: deviceLinkModel
						    textRole: "name"
						    displayText: currentIndex >= 0 ? currentText : "(none)"

						    onActivated: {
						        if (deviceLinkModel.count <= 0) return
						        const selected = deviceLinkModel.get(currentIndex)
						        printJobMultiInk.setDefaultDeviceLinkProfile(selected.path)
						    }
						}

						ThemedButton {
						    text: "Upload"
						    theme: root.theme
						    onClicked: {
						        iccDialogTarget = "deviceLink"
						        iccUploadDialog.open()
						    }
						}
					}
				}
			}


            // =========================
            // ICC Profiles (Nocai only)
            // =========================
            Pane {
                Layout.fillWidth: true
                padding: 16
                enabled: appState.usingSimulatedPrinter

                background: Rectangle {
                    color: theme.surface
                    radius: 12
                    border.width: 1
                    border.color: theme.divider
					opacity: appState.usingSimulatedPrinter ? 1.0 : 0.6
                }

                ColumnLayout {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: Math.min(parent.width, 520)
                    spacing: 12

                    Label {
                        text: "ICC Profiles (Nocai)"
                        color: theme.text
                        font.pixelSize: 18
                        font.weight: Font.Medium
                        Layout.alignment: Qt.AlignHCenter
                    }

                    Rectangle { height: 1; Layout.fillWidth: true; color: theme.divider; opacity: 0.8 }

                    Label { text: "Default Output ICC Profile"; color: theme.text; font.bold: true }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        ComboBox {
                            id: iccProfileDropdown
                            Layout.fillWidth: true
                            model: iccProfileModel
                            textRole: "name"
                            displayText: currentIndex >= 0 ? currentText : "(none)"

							onActivated: {
								if (!root.appState.usingSimulatedPrinter) return
								if (iccProfileModel.count <= 0) return
								if (!root.appState.selectedPrinter || root.appState.selectedPrinter.length <= 0) return

								const selected = iccProfileModel.get(currentIndex)
								const backend = root.activeBackend()
								if (backend) backend.setDefaultOutputICCProfile(selected.path)

								const inkMode = root.currentOutputProfileInkMode()
								const family = colorManager.outputProfileFamilyForInkMode(inkMode)
								colorManager.setPrinterFamilyOutputProfile(root.appState.selectedPrinter, family, selected.path)
							}
                        }

                        ThemedButton {
                            text: "Upload"
                            theme: root.theme
                            onClicked: { iccDialogTarget = "output"; iccUploadDialog.open() }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        Label { text: "Use Default Input CMYK"; color: theme.text; Layout.preferredWidth: 180 }

                        Switch {
                            id: useInputCmykSwitch
                            checked: root.activeBackend() ? root.activeBackend().checkDefaultInputCMYK() : false
                            onToggled: {
                                const backend = root.activeBackend()
                                if (backend) backend.enableDefaultInputCMYK(checked)
                            }
                        }
                    }

                    Label { text: "Default Input CMYK Profile"; color: theme.text; font.bold: true }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        ComboBox {
                            id: inputCmykDropdown
                            Layout.fillWidth: true
                            model: iccProfileModel
                            textRole: "name"
                            displayText: currentIndex >= 0 ? currentText : "(none)"
                            enabled: useInputCmykSwitch.checked

                            onActivated: {
                                if (!enabled) return
                                if (iccProfileModel.count <= 0) return
                                const selected = iccProfileModel.get(currentIndex)
                                const backend = root.activeBackend()
                                if (backend) backend.setDefaultInputCMYKProfile(selected.path)
                            }
                        }

                        ThemedButton {
                            text: "Upload"
                            theme: root.theme
                            enabled: useInputCmykSwitch.checked
                            onClicked: { iccDialogTarget = "inputCMYK"; iccUploadDialog.open() }
                        }
                    }
                    
                    Label {
						visible: appState.usingMultiInkPrinter
						text: "Linearization XML"
						color: theme.text
						font.bold: true
					}

					RowLayout {
						visible: appState.usingMultiInkPrinter
						Layout.fillWidth: true
						spacing: 10

						Label {
							Layout.fillWidth: true
							text: {
								const p = root.resolvedLinearizationForCurrentSelection()
								return (p && p.length > 0) ? p : "(none)"
							}
							color: theme.subtext
							wrapMode: Text.WrapAnywhere
						}

						ThemedButton {
							text: "Load"
							theme: root.theme
							onClicked: linearizationUploadDialog.open()
						}

						ThemedButton {
							text: "Clear"
							theme: root.theme
							enabled: root.resolvedLinearizationForCurrentSelection().length > 0
							onClicked: {
								if (!root.appState.selectedPrinter || root.appState.selectedPrinter.length <= 0)
									return

								const family = root.currentFamilyKey()
								colorManager.setPrinterFamilyLinearizationPath(root.appState.selectedPrinter, family, "")
								Qt.callLater(root.syncUIFromAppState)
								toast.show("Printer linearization override cleared.")
							}
						}
					}

                    FileDialog {
                        id: iccUploadDialog
						title: (iccDialogTarget === "deviceLink") ? "Select DeviceLink ICC" : "Select ICC Profile"
                        nameFilters: ["ICC Profiles (*.icc *.icm)", "All Files (*)"]
                        fileMode: FileDialog.OpenFile

                        onAccepted: {
							const path = normalizePath(iccUploadDialog.file.toString())
							const name = path.split("/").pop()
							const backend = root.activeBackend()

							if (iccDialogTarget === "deviceLink") {
								deviceLinkModel.append({ name: name, path: path })
								printJobMultiInk.addDeviceLinkProfile(name, path)
								deviceLinkDropdown.currentIndex = deviceLinkModel.count - 1
								printJobMultiInk.setDefaultDeviceLinkProfile(path)
								toast.show("DeviceLink added: " + name)
								return
							}

							iccProfileModel.append({ name: name, path: path })
							if (backend) backend.addICCProfile(name, path)

							if (iccDialogTarget === "output") {
								iccProfileDropdown.currentIndex = iccProfileModel.count - 1
								if (backend) backend.setDefaultOutputICCProfile(path)

								if (root.appState.selectedPrinter && root.appState.selectedPrinter.length > 0) {
									const inkMode = root.currentOutputProfileInkMode()
									const family = colorManager.outputProfileFamilyForInkMode(inkMode)
									colorManager.setPrinterFamilyOutputProfile(root.appState.selectedPrinter, family, path)
								}
							} else if (iccDialogTarget === "inputCMYK") {
								inputCmykDropdown.currentIndex = iccProfileModel.count - 1
								if (backend) backend.setDefaultInputCMYKProfile(path)
								colorManager.defaultInputProfile = path
							}

							toast.show("ICC added: " + name)
						}
                    }
                    
                    FileDialog {
						id: linearizationUploadDialog
						title: "Select Linearization XML"
						nameFilters: ["Linearization XML (*.xml)", "All Files (*)"]
						fileMode: FileDialog.OpenFile

						onAccepted: {
							const path = normalizePath(linearizationUploadDialog.file.toString())

							if (!root.appState.selectedPrinter || root.appState.selectedPrinter.length <= 0)
								return

							const family = root.currentFamilyKey()
							colorManager.setPrinterFamilyLinearizationPath(root.appState.selectedPrinter, family, path)

							Qt.callLater(root.syncUIFromAppState)
							toast.show("Printer linearization override updated.")
						}
					}
                }
            }

            // =========================
            // Selected Printer Details
            // =========================
            Pane {
                Layout.fillWidth: true
                padding: 16
                visible: hasPrinterSelected()

                background: Rectangle {
                    color: theme.surface
                    radius: 12
                    border.width: 1
                    border.color: theme.divider
                }

                ColumnLayout {
					anchors.horizontalCenter: parent.horizontalCenter
					width: Math.min(parent.width, 640)
					spacing: 10

                    Label {
                        text: "Selected Printer Details"
                        color: theme.text
                        font.pixelSize: 18
                        font.weight: Font.Medium
                        Layout.alignment: Qt.AlignHCenter
                    }

                    Rectangle { height: 1; Layout.fillWidth: true; color: theme.divider; opacity: 0.8 }

                    Label { text: "Name: " + appState.selectedPrinter; color: theme.text }
                    Label { text: "Nocai Printer: " + (appState.usingSimulatedPrinter ? "Yes" : "No"); color: theme.text }

                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: theme.text
                        text: "Supported Resolutions: " +
                              (appState.usingSimulatedPrinter
                               ? (nocaiPrinterCapabilities[appState.selectedPrinter] && nocaiPrinterCapabilities[appState.selectedPrinter].resolutions
                                  ? nocaiPrinterCapabilities[appState.selectedPrinter].resolutions.join(", ")
                                  : "(unknown)")
                               : printJobOutput.supportedResolutions().join(", "))
                    }

                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: theme.text
                        text: "Media Sizes: " +
                              (appState.usingSimulatedPrinter
                               ? (nocaiPrinterCapabilities[appState.selectedPrinter] && nocaiPrinterCapabilities[appState.selectedPrinter].mediaSizes
                                  ? nocaiPrinterCapabilities[appState.selectedPrinter].mediaSizes.join(", ")
                                  : "(unknown)")
                               : printJobOutput.supportedMediaSizes().join(", "))
                    }

                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: theme.text
                        text: "Duplex Modes: " +
                              (appState.usingSimulatedPrinter
                               ? (nocaiPrinterCapabilities[appState.selectedPrinter] && nocaiPrinterCapabilities[appState.selectedPrinter].duplexModes
                                  ? nocaiPrinterCapabilities[appState.selectedPrinter].duplexModes.join(", ")
                                  : "(unknown)")
                               : printJobOutput.supportedDuplexModes().join(", "))
                    }

                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: theme.text
                        text: "Color Modes: " +
                              (appState.usingSimulatedPrinter
                               ? (nocaiPrinterCapabilities[appState.selectedPrinter] && nocaiPrinterCapabilities[appState.selectedPrinter].colorModes
                                  ? nocaiPrinterCapabilities[appState.selectedPrinter].colorModes.join(", ")
                                  : "(unknown)")
                               : printJobOutput.supportedColorModes().join(", "))
                    }

	                    Label {
	                        visible: appState.usingSimulatedPrinter
	                        Layout.fillWidth: true
	                        wrapMode: Text.WordWrap
	                        color: theme.subtext
                        text: appState.usingMultiInkPrinter
	                              ? ("Ink Layout: " + appState.multiInkInkMode + " channels")
	                              : "Ink Layout: CMYK / CMYK+W via Nocai engine"
	                    }

                        Label {
                            visible: root.isX36MultiInk()
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            color: theme.subtext
                            text: "SDK Printer: " + (root.sdkSelectedPrinterName.length > 0 ? root.sdkSelectedPrinterName : "(none)")
                        }

                        Label {
                            visible: root.isX36MultiInk()
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            color: theme.subtext
                            text: "SDK Status: print=" +
                                  (root.sdkPrinterStatusInfo && root.sdkPrinterStatusInfo.ok ? root.sdkPrinterStatusInfo.printStatus : "(unknown)") +
                                  ", clean=" +
                                  (root.sdkPrinterStatusInfo && root.sdkPrinterStatusInfo.ok ? root.sdkPrinterStatusInfo.cleanStatus : "(unknown)")
                        }

                        Label {
                            visible: root.isX36MultiInk()
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            color: theme.subtext
                            text: "SDK Firmware: main FPGA " +
                                  (root.sdkPrinterFirmwareInfo && root.sdkPrinterFirmwareInfo.ok ? root.sdkPrinterFirmwareInfo.mainboardFpga : "(unknown)") +
                                  ", car FPGA " +
                                  (root.sdkPrinterFirmwareInfo && root.sdkPrinterFirmwareInfo.ok ? root.sdkPrinterFirmwareInfo.carboardFpga : "(unknown)") +
                                  ", main CPU " +
                                  (root.sdkPrinterFirmwareInfo && root.sdkPrinterFirmwareInfo.ok ? root.sdkPrinterFirmwareInfo.mainboardCpu : "(unknown)") +
                                  ", car CPU " +
                                  (root.sdkPrinterFirmwareInfo && root.sdkPrinterFirmwareInfo.ok ? root.sdkPrinterFirmwareInfo.carboardCpu : "(unknown)")
                        }
	                }
	            }

            Item { height: 6 }
        }
    }

    Toast {
        id: toast
        parent: Overlay.overlay
    }
}
