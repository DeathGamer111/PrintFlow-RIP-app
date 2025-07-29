import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.platform

Page {
    id: setupPage
    required property StackView stackView
    required property var appState

    Component.onCompleted: {
    	printJobOutput.refreshDetectedPrinters()
    }

    property var nocaiPrinterCapabilities: {
        "X-33": {
            resolutions: ["720x720", "720x1440", "720x2160"],
            mediaSizes: ["A1", "A2", "A3", "A4", "A5", "A6", "Tabloid"],
            duplexModes: ["None"],
            colorModes: ["CMYK", "CMYKWW", "CMYKWV"]
        }
    }
    
	ListModel {
		id: iccProfileModel
	}

    ColumnLayout {
	    anchors.fill: parent
		anchors.margins: 20
        spacing: 20
        Layout.alignment: Qt.AlignHCenter
        width: Math.min(parent.width, 450)

        Label {
            text: "Printer Setup"
            font.pixelSize: 22
            Layout.alignment: Qt.AlignHCenter
        }

        TabBar {
            id: printerTabs
            Layout.fillWidth: true
            TabButton { text: "Nocai Printer" }
            TabButton { text: "Network Printer" }
        }

        StackLayout {
            currentIndex: printerTabs.currentIndex
            Layout.fillWidth: true
    		Layout.fillHeight: true

            // === Nocai Printer Tab (Default) ===
            Item {
				Layout.fillWidth: true
				Layout.fillHeight: true
			    
                ColumnLayout {
                    spacing: 10
                    anchors.fill: parent
                    Layout.alignment: Qt.AlignHCenter
                    
					Label {
						text: "Please Select your Nocai Printer"
						font.bold: true
						Layout.alignment: Qt.AlignCenter
					}

                    ComboBox {
                        id: nocaiPrinterComboBox
						Layout.preferredWidth: 150
						Layout.alignment: Qt.AlignCenter
                        model: ["X-33"]

                        onActivated: {
                            const selected = nocaiPrinterComboBox.currentText
                            if (selected.length > 0) {
                                appState.selectedPrinter = selected
                                appState.usingSimulatedPrinter = true

             					// Prepare Nocai assets first
                                printJobNocai.prepareNocaiAssets()

                                // Reload ICC profiles and select the default
								iccProfileModel.clear()
								const profiles = printJobNocai.getAvailableICCProfiles()

								for (let i = 0; i < profiles.length; ++i) {
									iccProfileModel.append(profiles[i])
								}

								const currentDefault = printJobNocai.getDefaultOutputICCProfile()

								for (let i = 0; i < iccProfileModel.count; ++i) {
									if (iccProfileModel.get(i).path === currentDefault) {
										Qt.callLater(() => {
											iccProfileDropdown.currentIndex = i
										})
										break
									}
								}

                                toast.show("Nocai printer selected: " + selected)
                            }
                        }
                    }

					ColumnLayout {
						spacing: 10
		                Layout.fillWidth: true
		                Layout.alignment: Qt.AlignHCenter

						Label {
							text: " Select Default Output ICC Profile"
							font.bold: true
							Layout.alignment: Qt.AlignCenter
						}

						RowLayout {
							spacing: 10
							Layout.alignment: Qt.AlignHCenter
							
							ComboBox {
								id: iccProfileDropdown
								Layout.alignment: Qt.AlignVCenter
								Layout.preferredWidth: 175
								model: iccProfileModel
								textRole: "name"

								onCurrentIndexChanged: {
									if (iccProfileModel.count > 0 && iccProfileDropdown.currentIndex >= 0) {
										let selected = iccProfileModel.get(iccProfileDropdown.currentIndex)
										printJobNocai.setDefaultOutputICCProfile(selected.path)
									}
								}
							}

							Button {
								text: "Upload ICC Profile"
								Layout.alignment: Qt.AlignVCenter
								onClicked: iccUploadDialog.open()
							}
						}

						FileDialog {
							id: iccUploadDialog
							title: "Select ICC Profile"
							nameFilters: ["ICC Profiles (*.icc *.icm)", "All Files (*)"]
							fileMode: FileDialog.OpenFile

							onAccepted: {
								let url = iccUploadDialog.file.toString()
								let path = url.startsWith("file://") ? url.slice(7) : url
								let name = path.split("/").pop()

								iccProfileModel.append({ name: name, path: path })
								iccProfileDropdown.currentIndex = iccProfileModel.count - 1

								printJobNocai.addICCProfile(name, path)
								printJobNocai.setDefaultOutputICCProfile(path)
							}
						}
					}

                    ColumnLayout {
                        spacing: 10
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignHCenter

                        Label {
                            text: "The Nocai engine generates PRN files for USB or offline transfer to a supported Nocai printer or Atools software."
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                            Layout.maximumWidth: 400
                        }

                    }
                }
            }

            // === Network Printer Tab ===
            Item {
				Layout.fillWidth: true
				Layout.fillHeight: true
        
                ColumnLayout {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: 10
                    anchors.fill: parent

                    ComboBox {
                        id: printerComboBox
                        Layout.fillWidth: true
                        model: printJobOutput.detectedPrinters

                        onActivated: {
                            const name = printerComboBox.currentText
                            if (printJobOutput.loadPrinter(name)) {
                                appState.selectedPrinter = name
                                appState.usingSimulatedPrinter = false

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

                    Button {
                        text: "Refresh List"
                        Layout.alignment: Qt.AlignHCenter
                        onClicked: printJobOutput.refreshDetectedPrinters()
                    }
                }
            }
        }

        // === Printer Info (after setup) ===
        GroupBox {
            visible: appState.selectedPrinter.length > 0
            Layout.fillWidth: true

            ColumnLayout {
                spacing: 6
                Layout.fillWidth: true

				Label {
					text: "Selected Printer Details"
					font.bold: true
					font.pixelSize: 16
					Layout.alignment: Qt.AlignLeft
				}
				
                Label { text: "Name: " + appState.selectedPrinter }
                Label { text: "Nocai Printer: " + (appState.usingSimulatedPrinter ? "Yes" : "No") }

                // Capabilities based on printer type
                Label {
                    Layout.preferredWidth: parent.width
                    Layout.maximumWidth: 480
                    text: "Supported Resolutions: " +
                        (appState.usingSimulatedPrinter
                         ? nocaiPrinterCapabilities[appState.selectedPrinter]?.resolutions?.join(", ")
                         : printJobOutput.supportedResolutions().join(", "))
                }
                Label {
                    Layout.preferredWidth: parent.width
                    Layout.maximumWidth: 480
                    text: "Media Sizes: " +
                        (appState.usingSimulatedPrinter
                         ? nocaiPrinterCapabilities[appState.selectedPrinter]?.mediaSizes?.join(", ")
                         : printJobOutput.supportedMediaSizes().join(", "))
                }
                Label {
                    Layout.preferredWidth: parent.width
                    Layout.maximumWidth: 480
                    text: "Duplex Modes: " +
                        (appState.usingSimulatedPrinter
                         ? nocaiPrinterCapabilities[appState.selectedPrinter]?.duplexModes?.join(", ")
                         : printJobOutput.supportedDuplexModes().join(", "))
                }
                Label {
                    Layout.preferredWidth: parent.width
                    Layout.maximumWidth: 480
                    text: "Color Modes: " +
                        (appState.usingSimulatedPrinter
                         ? nocaiPrinterCapabilities[appState.selectedPrinter]?.colorModes?.join(", ")
                         : printJobOutput.supportedColorModes().join(", "))
                }
            }
        }

        // === Confirm / Cancel ===
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 20

            Button {
                text: "Confirm Setup"
                enabled: appState.selectedPrinter.length > 0
                onClicked: {
                    toast.show("Printer setup complete: " + appState.selectedPrinter)
                    stackView.pop()
                }
            }

            Button {
                text: "Cancel"
                onClicked: {
                    stackView.pop()
                }
            }
        }
    }

    Toast {
        id: toast
        parent: Overlay.overlay
    }
}
