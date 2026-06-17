import QtQuick
import QtQuick.Controls
import QtQuick.Controls as C
import QtQuick.Layouts
import Qt.labs.platform as P
import QtCore
import QtQuick.Window
import "."


// Job list view: selection workflow, persistence (load/save), and PRN generation entry.
Item {
    id: root
    required property StackView stackView
    required property var jobModel
    required property var appState
    required property Theme theme

    // Selection state used by toolbar actions and list highlighting.
    property bool selectionMode: false
    property var selectedIndexes: []
    property string suggestedFilename: ""

    width: parent.width
    height: parent.height
    
    Component.onCompleted: {
    	colorManager.selectedPrinter = appState.selectedPrinter
	}
	
	Connections {
		target: appState
		function onSelectedPrinterChanged() {
			colorManager.selectedPrinter = appState.selectedPrinter
		}
	}

	// Toggle a single index in/out of the selection.
    function toggleSelection(index) {
            const exists = selectedIndexes.includes(index)
            const updated = selectedIndexes.slice()

            if (exists) {
                const i = updated.indexOf(index)
                updated.splice(i, 1)
            } else {
                updated.push(index)
            }

            selectedIndexes = updated
	}

		
	// Readable check for whether an index is selected.
    function isSelected(index) {
        return selectedIndexes.includes(index)
    }


	// True only if every job is selected (keeps toolbar label in sync).
    function areAllJobsSelected() {
        if (selectedIndexes.length !== jobModel.count)
            return false

        for (let i = 0; i < jobModel.count; ++i) {
            if (selectedIndexes.indexOf(i) === -1)
                return false
        }
        return true
    }


	// Select all jobs currently in the model.
    function selectAll() {
        const total = jobModel.count
        if (total === 0)
            return

        const all = []
        for (let i = 0; i < total; ++i) {
            all.push(i)
        }

        selectedIndexes = all
    }


    // Clear multi-selection.
    function deselectAll() {
        selectedIndexes = []
    }


    function directPrintSettings() {
        return {
            selectedPrinterIndex: appState.sdkSelectedPrinterIndex,
            printDirection: appState.sdkPrintDirection,
            printSpeed: appState.sdkPrintSpeed,
            wcSequence: appState.sdkWcSequence,
            eclosionGrade: appState.sdkEclosionGrade,
            headSelect: appState.sdkHeadSelect,
            whiteInkPercent: appState.sdkWhiteInkPercent,
            whiteInkPassCount: appState.sdkWhiteInkPassCount,
            varnishInkPercent: appState.sdkVarnishInkPercent,
            varnishInkPassCount: appState.sdkVarnishInkPassCount,
            headVoltage: appState.sdkHeadVoltage,
            disableUv0: appState.sdkDisableUv0,
            disableUv1: appState.sdkDisableUv1,
            disableUv2: appState.sdkDisableUv2,
            disableUv3: appState.sdkDisableUv3,
            disableUv4: appState.sdkDisableUv4,
            disableUv5: appState.sdkDisableUv5,
            carReset: appState.sdkCarReset,
            stripBlank: appState.sdkStripBlank,
            blankDistance: appState.sdkBlankDistance,
            pass: appState.sdkPass,
            vsdMode: appState.sdkVsdMode
        }
    }


    function printSelectedMultiInkDirectly() {
        if (!appState.supportsDirectPrint || !appState.supportsRipProcessing) {
            toast.show("Direct print is not available on this platform yet.")
            return
        }

        const job = jobModel.getJob(selectedIndexes[0])
        var directJob = Object.assign({}, job)
        directJob.inkMode = appState.multiInkInkMode
        directJob.directPrintSettings = directPrintSettings()
        appState.isGeneratingPRN = true
        console.log("Routing to Nocai MultiInk direct print backend with inkMode =", appState.multiInkInkMode)
        printJobMultiInk.runDirectPrint(directJob)
    }


	// Direct print path (bypasses file save): generates and sends to the configured printer.
    function printSelectedJobDirectly() {
        if (!appState.supportsCupsPrinting) {
            toast.show("CUPS printing is not available on this platform.")
            return
        }

        const index = selectedIndexes[0]
        const job = jobModel.getJob(index)
        const inputFile = job.imagePath

        const outputPath = "" // Empty because printing directly to printer

        const success = printJobOutput.generatePRN(job, inputFile, outputPath)
        if (success) {
            console.log("Print job sent to printer:", appState.selectedPrinter)
            toast.show("Print job sent successfully.")
        } else {
            console.warn("Failed to print job:", job.name)
            toast.show("Failed to print job.")
        }
    }
    
	function handlePrnFinished(success) {
		if (!appState.isGeneratingPRN)
			return

		appState.isGeneratingPRN = false

		if (success) {
            if (appState.usingMultiInkPrinter && appState.multiInkOutputMode === "direct") {
                console.log("Direct print sent successfully.")
                toast.show("Sent to printer.")
            } else {
                console.log("PRN generated successfully:", outputFileDialog.file)
                toast.show("PRN generated successfully.")
            }
		} else {
            if (appState.usingMultiInkPrinter && appState.multiInkOutputMode === "direct") {
                console.warn("Failed to send direct print job.")
                toast.show("Failed to send to printer.")
            } else {
                console.warn("Failed to generate PRN file.")
                toast.show("Failed to generate PRN file.")
            }
		}
	}


	// Main vertical layout for header, toolbars, list, and dialogs.
    ColumnLayout {
        width: parent.width
    	height: parent.height
        spacing: 0

        // Top branding/header with nav to printer setup.
        Rectangle {
			height: 60
			Layout.fillWidth: true
			color: theme.surface

            RowLayout {
				anchors.fill: parent
				anchors.leftMargin: 8
				anchors.rightMargin: 8
				spacing: 10

				// Left: logo
				Image {
					source: "qrc:/assets/logo.png"
					Layout.preferredWidth: 40
					Layout.preferredHeight: 40
					Layout.alignment: Qt.AlignVCenter
					fillMode: Image.PreserveAspectFit
					smooth: true
				}
				
				// Center: title (true centered)
    			Item { Layout.fillWidth: true }  // spacer

                Label {
					text: "Job Manager"
					font.pixelSize: 22
					color: theme.text
					horizontalAlignment: Text.AlignHCenter
					verticalAlignment: Text.AlignVCenter
					Layout.alignment: Qt.AlignVCenter
				}

				Item { Layout.fillWidth: true }  // spacer

				// Right: settings
				C.ToolButton {
					id: settingsBtn
					text: "Settings"
					Layout.alignment: Qt.AlignVCenter
					hoverEnabled: true
					
				    height: 36
				    padding: 10
					
					background: Rectangle {
						radius: 8
						color: settingsBtn.pressed
							   ? Qt.rgba(root.theme.accent2.r, root.theme.accent2.g, root.theme.accent2.b, 0.22)
							   : (settingsBtn.hovered
								    ? Qt.rgba(root.theme.text.r, root.theme.text.g, root.theme.text.b, 0.10)
								    : "transparent")
						border.width: settingsBtn.hovered || settingsBtn.pressed ? 1 : 0
						border.color: root.theme.divider
					}

					contentItem: Label {
						text: settingsBtn.text
						color: theme.text
						verticalAlignment: Text.AlignVCenter
						horizontalAlignment: Text.AlignHCenter
						elide: Text.ElideRight
					}

					onClicked: {
						const host = root.Window.window ? root.Window.window.contentItem : root
						const p = settingsBtn.mapToItem(host, 0, settingsBtn.height)

						settingsMenu.parent = host
						settingsMenu.x = Math.max(8, p.x + settingsBtn.width - settingsMenu.implicitWidth)
						settingsMenu.y = p.y + 6
						settingsMenu.open()
					}
				}


				C.Menu {
					id: settingsMenu
					padding: 6
					parent: root.Window.window ? root.Window.window.contentItem : root
					modal: false

					background: Rectangle {
						color: theme.surface2
						radius: 10
						implicitWidth: 240
						implicitHeight: contentItem ? contentItem.implicitHeight + 12 : 120
						border.width: 1
						border.color: theme.divider
					}

						C.MenuItem {
							id: miLoad
							text: "Load Job(s)"
							hoverEnabled: true

							background: Rectangle {
								radius: 8
								color: miLoad.pressed
									   ? Qt.rgba(root.theme.accent2.r, root.theme.accent2.g, root.theme.accent2.b, 0.25)
									   : (miLoad.hovered
											? Qt.rgba(root.theme.text.r, root.theme.text.g, root.theme.text.b, 0.12)
											: "transparent")
							}

							contentItem: Label { text: miLoad.text; color: root.theme.text; verticalAlignment: Text.AlignVCenter }

							onTriggered: {
								settingsMenu.close()
								openFileDialog.open()
							}
						}

						C.MenuSeparator { }

						C.MenuItem {
							id: miPrinter
							text: "Printer Setup"
							hoverEnabled: true

							background: Rectangle {
								radius: 8
								color: miPrinter.pressed
									   ? Qt.rgba(root.theme.accent2.r, root.theme.accent2.g, root.theme.accent2.b, 0.25)
									   : (miPrinter.hovered
											? Qt.rgba(root.theme.text.r, root.theme.text.g, root.theme.text.b, 0.12)
											: "transparent")
							}

							contentItem: Label { text: miPrinter.text; color: root.theme.text; verticalAlignment: Text.AlignVCenter }

							onTriggered: {
								settingsMenu.close()
								stackView.push("qrc:/qml/PrinterSetupView.qml", {
									stackView: stackView,
									appState: appState,
									theme: root.theme
								})
							}
						}
						
						C.MenuSeparator { }

						C.MenuItem {
							id: miMaintenance
							text: "Printer Maintenance"
							enabled: nocaiDirectPrint.supportsMaintenance(appState.selectedPrinter)
							hoverEnabled: enabled

							background: Rectangle {
								radius: 8
								color: miMaintenance.pressed
									   ? Qt.rgba(root.theme.accent2.r, root.theme.accent2.g, root.theme.accent2.b, 0.25)
									   : (miMaintenance.hovered
											? Qt.rgba(root.theme.text.r, root.theme.text.g, root.theme.text.b, 0.12)
											: "transparent")
							}

							contentItem: Label {
								text: miMaintenance.text
								color: miMaintenance.enabled ? root.theme.text : root.theme.subtext
								opacity: miMaintenance.enabled ? 1.0 : 0.55
								verticalAlignment: Text.AlignVCenter
							}

							onTriggered: {
								settingsMenu.close()
								stackView.push("qrc:/qml/PrinterMaintenanceView.qml", {
									stackView: stackView,
									appState: appState,
									theme: root.theme
								})
							}
						}

						C.MenuSeparator { }

						C.MenuItem {
							id: miColor
							text: "Color Management"
							hoverEnabled: true

							background: Rectangle {
								radius: 8
								color: miColor.pressed
									   ? Qt.rgba(root.theme.accent2.r, root.theme.accent2.g, root.theme.accent2.b, 0.25)
									   : (miColor.hovered
											? Qt.rgba(root.theme.text.r, root.theme.text.g, root.theme.text.b, 0.12)
											: "transparent")
							}

							contentItem: Label { text: miColor.text; color: root.theme.text; verticalAlignment: Text.AlignVCenter }

							onTriggered: {
								settingsMenu.close()
								stackView.push("qrc:/qml/ColorManagementView.qml", {
									stackView: stackView,
									appState: appState,
									theme: root.theme
								})
							}
						}

						C.MenuSeparator { }

						C.MenuItem {
							id: miDarkMode
							text: root.theme.dark ? "Switch to Light Mode" : "Switch to Dark Mode"
							hoverEnabled: true

							background: Rectangle {
								radius: 8
								color: miDarkMode.pressed
									   ? Qt.rgba(root.theme.accent2.r, root.theme.accent2.g, root.theme.accent2.b, 0.25)
									   : (miDarkMode.hovered
											? Qt.rgba(root.theme.text.r, root.theme.text.g, root.theme.text.b, 0.12)
											: "transparent")
							}
							contentItem: Label { text: miDarkMode.text; color: root.theme.text; verticalAlignment: Text.AlignVCenter }
							onTriggered: root.theme.dark = !root.theme.dark
						}
				}
            }
        }


        // Toolbar controlling selection lifecycle and save/remove actions.
        Frame {
            Layout.fillWidth: true
            padding: 10
			background: Rectangle { color: theme.surface2 }

            RowLayout {
				anchors.fill: parent
				anchors.leftMargin: 8
				anchors.rightMargin: 8
				spacing: 10

                
                ThemedButton {
					text: "New Job"
					visible: !selectionMode
					theme: root.theme
					padding: 12
					font.pixelSize: 15
					onClicked: jobModel.addJob("New Print Job")
				}
				
		        Item { Layout.fillWidth: true; visible: !selectionMode }

				ThemedButton {
					text: "Select Jobs"
					visible: !selectionMode
					theme: root.theme
					padding: 12
					font.pixelSize: 15
					onClicked: {
						selectedIndexes = []
						selectionMode = true
					}
				}

				// Selection mode (Cancel + actions)
				ThemedButton {
					text: "Cancel Selection"
					visible: selectionMode
					theme: root.theme
					onClicked: {
						selectedIndexes = []
						selectionMode = false
					}
				}

				ThemedButton {
					text: areAllJobsSelected() ? "Deselect All" : "Select All"
					visible: selectionMode
					theme: root.theme
					onClicked: areAllJobsSelected() ? deselectAll() : selectAll()
				}

				ThemedButton {
					text: "Remove Jobs"
					visible: selectionMode
					enabled: selectedIndexes.length > 0
					theme: root.theme
					onClicked: {
						const sorted = selectedIndexes.slice().sort((a, b) => b - a)
						for (let i = 0; i < sorted.length; ++i)
							jobModel.removeJob(sorted[i])
						selectedIndexes = []
					}
				}

				// Save selected jobs to JSON (with embedded base64 image data).
                ThemedButton {
                    text: "Save Jobs"
                    visible: selectionMode
                    enabled: selectedIndexes.length > 0
       				theme: root.theme
                    onClicked: {
                        let jobName = jobModel.getJob(selectedIndexes[0]).name || "UntitledJob"
                        const downloads = StandardPaths.writableLocation(StandardPaths.DownloadLocation)
                        const fullPath = downloads + "/" + jobName.replace(/[^a-zA-Z0-9_-]/g, "_") + ".json"
                        saveFileDialog.currentFile = fullPath
                        saveFileDialog.open()
                    }
                }
            }
        }


        // Selection status readout; shown only while selection mode is active.
        Label {
            visible: selectionMode
            text: selectedIndexes.length + " job(s) selected"
            font.pixelSize: 14
			color: theme.subtext
            Layout.topMargin: 10
            Layout.bottomMargin: 10
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
        }


        // Scrollable job list with click-to-open or click-to-toggle-select behavior.
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 12
            clip: true
            
            ScrollBar.vertical.policy: ScrollBar.AlwaysOff
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ListView {
                id: jobListView
                width: Math.min(parent.width, 500)
                model: jobModel
                spacing: 6
                Layout.alignment: Qt.AlignHCenter

				// Each row supports selection highlight and navigation into details.
                delegate: ItemDelegate {
                    width: jobListView.width
                    hoverEnabled: true
                    highlighted: selectionMode && isSelected(index)
                    
					background: Rectangle {
						radius: 10
						color: highlighted
							   ? Qt.rgba(root.theme.accent.r, root.theme.accent.g, root.theme.accent.b, 0.18)
							   : (parent.hovered
								    ? Qt.rgba(root.theme.text.r, root.theme.text.g, root.theme.text.b, 0.08)
								    : "transparent")
						border.width: highlighted ? 1 : 0
						border.color: root.theme.accent
					}

                    onClicked: {
                        if (selectionMode) {
                            toggleSelection(index)
                        } else {
                            stackView.push("qrc:/qml/JobDetailsView.qml", {
                                jobIndex: index,
	                                stackView: stackView,
	                                appState: appState,
	                                jobModel: jobModel,
	                                theme: root.theme
	                            })
                        }
                    }


					// Row content: optional checkbox + job name.
                    contentItem: RowLayout {
						Layout.fillWidth: true
						Layout.preferredHeight: parent.height
                        spacing: 10

                        CheckBox {
                            visible: selectionMode
                            checked: isSelected(index)
                            onToggled: toggleSelection(index)
                        }

                        Label {
                            text: model.name
                            Layout.fillWidth: true
                            verticalAlignment: Text.AlignVCenter
                            color: theme.subtext
                        }
                    }
                }
            }
        }

        // Bottom toolbar for creating/loading jobs and kicking off print/PRN.
        Rectangle {
            Layout.fillWidth: true
            height: 50
			color: theme.surface

            RowLayout {
                anchors.centerIn: parent
                spacing: 20

				// Print entry point: either open save dialog (simulated printer) or send directly.
                ThemedButton {
                    text: "Print Job"
					visible: selectionMode
					enabled: selectedIndexes.length > 0
					theme: root.theme

					padding: 14
					font.pixelSize: 16
					
                    onClicked: {                    
						const job = jobModel.getJob(selectedIndexes[0])
						const jobName = job.name || "UntitledJob"
						const downloads = StandardPaths.writableLocation(StandardPaths.DownloadLocation)
						const fullPath = downloads + "/" + jobName.replace(/[^a-zA-Z0-9_-]/g, "_") + ".prn"
						outputFileDialog.currentFile = fullPath

                        if (appState.usingSimulatedPrinter) {
                            if (appState.usingMultiInkPrinter && appState.multiInkOutputMode === "direct")
                                printSelectedMultiInkDirectly()
                            else
                                outputFileDialog.open()
                        } else {
                            printSelectedJobDirectly()
                        }
                    }
                }
            }
        }


        // File dialog for loading job JSON.
        P.FileDialog {
            id: openFileDialog
            title: "Load Jobs from JSON"
            nameFilters: ["JSON Files (*.json)"]
            fileMode: P.FileDialog.OpenFile
            onAccepted: jobModel.loadFromJson(file)
        }


		// File dialog for saving selected jobs to JSON.
        P.FileDialog {
            id: saveFileDialog
            title: "Save Jobs to JSON"
            nameFilters: ["JSON Files (*.json)"]
            fileMode: P.FileDialog.SaveFile
            defaultSuffix: "json"
            onAccepted: jobModel.saveToJson(file, selectedIndexes)
        }


		// File dialog for choosing PRN output path when using the simulated printer.
        P.FileDialog {
            id: outputFileDialog
            title: "Select PRN File Destination"
            nameFilters: ["PRN Files (*.prn)", "All Files (*)"]
            fileMode: P.FileDialog.SaveFile
	    	defaultSuffix: "prn"
            onAccepted: {
                const outputPath = file
                const job = jobModel.getJob(selectedIndexes[0])

                appState.isGeneratingPRN = true
				
				if (appState.usingMultiInkPrinter == true) {
                    if (!appState.supportsRipProcessing) {
                        toast.show("PRN generation is not available on this platform yet.")
                        appState.isGeneratingPRN = false
                        return
                    }

				    // Clone the job map so we can inject MultiInk-specific fields
					var multiInkJob = Object.assign({}, job)

					// Pass current ink mode into the pipeline
					multiInkJob.inkMode = appState.multiInkInkMode
                    multiInkJob.directPrintSettings = directPrintSettings()

					console.log("Routing to Nocai MultiInk backend with inkMode =", appState.multiInkInkMode)
					printJobMultiInk.runPRNGeneration(multiInkJob, outputPath)
				}
				else {
                    if (!appState.supportsRipProcessing) {
                        toast.show("PRN generation is not available on this platform yet.")
                        appState.isGeneratingPRN = false
                        return
                    }

					console.log("Routing to standard Nocai backend")
			        printJobCMYK.runPRNGeneration(job, outputPath)
				}
            }
        }

		// Connect to PRN completion signal to update UI and notify user.
		Connections {
			target: printJobCMYK

			function onPrnGenerationFinished(success) {
				root.handlePrnFinished(success)
			}
		}

		Connections {
			target: printJobMultiInk

			function onPrnGenerationFinished(success) {
				root.handlePrnFinished(success)
			}
		}

		// Lightweight toast for transient feedback.
        Toast {
            id: toast
            parent: Overlay.overlay
        }
    }
    
    
    // Full-screen progress overlay while PRN generation runs.
    Item {
        id: spinnerOverlay
		anchors.fill: parent
        visible: appState.isGeneratingPRN
        z: 999

        Rectangle {
            anchors.fill: parent
            color: "#00000088"


			// Simple circular spinner built from rectangles with rotation animation.
			Item {
				width: 64
				height: 64
				anchors.horizontalCenter: parent.horizontalCenter
				anchors.verticalCenter: parent.verticalCenter
                transformOrigin: Item.Center

				RotationAnimator on rotation {
					from: 0
					to: 360
					duration: 1000
					loops: Animation.Infinite
					running: spinnerOverlay.visible
				}

				Rectangle {
					anchors.fill: parent
					radius: width / 2
					border.width: 6
					border.color: "#402DD4BF"
					color: "transparent"
				}

				Rectangle {
					width: 6
					height: parent.height / 2
					anchors.top: parent.top
					anchors.horizontalCenter: parent.horizontalCenter
					color: theme.accent
                    radius: width / 2
				}
			}

            Text {
                text: "Rastering Image and Generating PRN..."
                anchors.top: parent.verticalCenter
                anchors.horizontalCenter: parent.horizontalCenter
				color: theme.subtext
                font.pixelSize: 18
                anchors.topMargin: 80
            }
        }
    }
}
