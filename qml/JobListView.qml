import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.platform
import QtCore


// Job list view: selection workflow, persistence (load/save), and PRN generation entry.
Item {
    id: root
    required property StackView stackView
    required property var jobModel
    required property var appState

    // Selection state used by toolbar actions and list highlighting.
    property bool selectionMode: false
    property var selectedIndexes: []
    property string suggestedFilename: ""

    width: parent.width
    height: parent.height

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


	// Direct print path (bypasses file save): generates and sends to the configured printer.
    function printSelectedJobDirectly() {
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


	// Main vertical layout for header, toolbars, list, and dialogs.
    ColumnLayout {
        width: parent.width
    	height: parent.height
        spacing: 0

        // Top branding/header with nav to printer setup.
        Rectangle {
            height: 60
            width: parent.width
            color: "#2c3e50"
            Layout.fillWidth: true

            RowLayout {
				Layout.fillWidth: true
				Layout.fillHeight: true
				spacing: 15

                Image {
                    source: "qrc:/assets/logo.png"
                    Layout.preferredWidth: 40
                    Layout.preferredHeight: 40
                    Layout.margins: 4
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                }

                Label {
                    text: "Print Job Manager"
                    font.pixelSize: 22
                    color: "white"
                    verticalAlignment: Text.AlignVCenter
                }

                Item { Layout.fillWidth: true }

                Button {
                    text: "Printer Setup"
                    Layout.leftMargin: 30
                    onClicked: stackView.push("qrc:/qml/PrinterSetupView.qml", {
                        stackView: stackView,
                        appState: appState
                    })
                }
            }
        }


        // Toolbar controlling selection lifecycle and save/remove actions.
        Frame {
            Layout.fillWidth: true
            padding: 10
            background: Rectangle { color: "#34495e" }

            RowLayout {
                spacing: 10
                Layout.alignment: Qt.AlignHCenter

                Button {
                    text: selectionMode ? "Cancel Selection" : "Select Jobs"
                    onClicked: {
                        selectedIndexes = []
                        selectionMode = !selectionMode
                    }
                }

                Button {
                    text: areAllJobsSelected() ? "Deselect All" : "Select All"
                    visible: selectionMode
                    onClicked: areAllJobsSelected() ? deselectAll() : selectAll()
                }

                Button {
                    text: "Remove Jobs"
                    visible: selectionMode
                    enabled: selectedIndexes.length > 0
                    onClicked: {
                        const sorted = selectedIndexes.slice().sort((a, b) => b - a)
                        for (let i = 0; i < sorted.length; ++i)
                            jobModel.removeJob(sorted[i])
                        selectedIndexes = []
                    }
                }

				// Save selected jobs to JSON (with embedded base64 image data).
                Button {
                    text: "Save Jobs"
                    visible: selectionMode
                    enabled: selectedIndexes.length > 0
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
            color: "#7f8c8d"
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
                    highlighted: selectionMode && isSelected(index)

                    onClicked: {
                        if (selectionMode) {
                            toggleSelection(index)
                        } else {
                            stackView.push("qrc:/qml/JobDetailsView.qml", {
                                jobIndex: index,
                                stackView: stackView,
                                appState: appState,
                                jobModel: jobModel
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
                        }
                    }
                }
            }
        }

        // Bottom toolbar for creating/loading jobs and kicking off print/PRN.
        Rectangle {
            Layout.fillWidth: true
            height: 50
            color: "#34495e"

            RowLayout {
                anchors.centerIn: parent
                spacing: 20

                Button {
                    text: "Add New Job"
                    onClicked: jobModel.addJob("New Print Job")
                }

                Button {
                    text: "Load Jobs"
                    onClicked: openFileDialog.open()
                }

				// Print entry point: either open save dialog (simulated printer) or send directly.
                Button {
                    text: "Print Job"
                    enabled: selectedIndexes.length > 0
                    onClicked: {                    
						const job = jobModel.getJob(selectedIndexes[0])
						const jobName = job.name || "UntitledJob"
						const downloads = StandardPaths.writableLocation(StandardPaths.DownloadLocation)
						const fullPath = downloads + "/" + jobName.replace(/[^a-zA-Z0-9_-]/g, "_") + ".prn"
						outputFileDialog.currentFile = fullPath

                        appState.usingSimulatedPrinter
                            ? outputFileDialog.open()
                            : printSelectedJobDirectly()
                    }
                }
            }
        }


        // File dialog for loading job JSON.
        FileDialog {
            id: openFileDialog
            title: "Load Jobs from JSON"
            nameFilters: ["JSON Files (*.json)"]
            fileMode: FileDialog.OpenFile
            onAccepted: jobModel.loadFromJson(file)
        }


		// File dialog for saving selected jobs to JSON.
        FileDialog {
            id: saveFileDialog
            title: "Save Jobs to JSON"
            nameFilters: ["JSON Files (*.json)"]
            fileMode: FileDialog.SaveFile
            defaultSuffix: "json"
            onAccepted: jobModel.saveToJson(file, selectedIndexes)
        }


		// File dialog for choosing PRN output path when using the simulated printer.
        FileDialog {
            id: outputFileDialog
            title: "Select PRN File Destination"
            nameFilters: ["PRN Files (*.prn)", "All Files (*)"]
            fileMode: FileDialog.SaveFile
	    defaultSuffix: "prn"
            onAccepted: {
                const outputPath = file
                const job = jobModel.getJob(selectedIndexes[0])

                appState.isGeneratingPRN = true

                // Use threaded function implemented in C++
                printJobNocai.runPRNGeneration(job, outputPath)
            }
        }


		// Connect to PRN completion signal to update UI and notify user.
        Connections {
            target: printJobNocai

            function onPrnGenerationFinished(success) {
                appState.isGeneratingPRN = false
                
                if (success) {
                    console.log("PRN generated successfully:", outputFileDialog.file)
                    toast.show("PRN generated successfully.")
                } else {
                    console.warn("Failed to generate PRN file.")
                    toast.show("Failed to generate PRN file.")
                }
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

				RotationAnimator on rotation {
					from: 0
					to: 360
					duration: 1000
					loops: Animation.Infinite
					running: true
				}

				Rectangle {
					anchors.fill: parent
					radius: width / 2
					border.width: 6
					border.color: "#3498db"
					color: "transparent"
				}

				Rectangle {
					width: 6
					height: height / 2
					anchors.top: parent.top
					anchors.horizontalCenter: parent.horizontalCenter
					color: "#3498db"
				}
			}

            Text {
                text: "Rastering Image and Generating PRN..."
                anchors.top: parent.verticalCenter
                anchors.horizontalCenter: parent.horizontalCenter
                color: "white"
                font.pixelSize: 18
                anchors.topMargin: 80
            }
        }
    }
}
