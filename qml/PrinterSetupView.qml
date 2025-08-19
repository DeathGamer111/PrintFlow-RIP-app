import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.platform


// Printer configuration screen: supports simulated (Nocai) and network printers,
// ICC profile selection/registration, and capability display.
Page {
    id: setupPage
    required property StackView stackView
    required property var appState


	// Populate detected network printers when the page loads.
    Component.onCompleted: {
    	printJobOutput.refreshDetectedPrinters()
    }


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
        }
    }
    
    
    // Tracks which ICC dropdown to update after a file is chosen.
    property string iccDialogTarget: "output" // "output" | "inputCMYK"
    
    
    // In-memory ICC list for dropdowns; populated from backend and user uploads.
	ListModel {
		id: iccProfileModel
	}


	// Main layout wrapper with centered content and modest max width.
    ColumnLayout {
	    anchors.fill: parent
		anchors.margins: 20
        spacing: 20
        Layout.alignment: Qt.AlignHCenter
        width: Math.min(parent.width, 450)

		// Title
        Label {
            text: "Printer Setup"
            font.pixelSize: 22
            Layout.alignment: Qt.AlignHCenter
        }

		// Top-level mode switcher between simulated Nocai and network printers.
        TabBar {
            id: printerTabs
            Layout.fillWidth: true
            TabButton { text: "Nocai Printer" }
            TabButton { text: "Network Printer" }
        }

		// Container that switches content based on the selected tab.
        StackLayout {
            currentIndex: printerTabs.currentIndex
            Layout.fillWidth: true
    		Layout.fillHeight: true


            // Nocai (simulated) printer setup.
            Item {
				Layout.fillWidth: true
				Layout.fillHeight: true
			    
			    // Nocai selection and ICC defaults.
                ColumnLayout {
                    spacing: 10
                    anchors.fill: parent
                    Layout.alignment: Qt.AlignHCenter
                    
					Label {
						text: "Please Select your Nocai Printer"
						font.bold: true
						Layout.alignment: Qt.AlignCenter
					}

					// Choosing a Nocai model initializes assets and loads ICCs.
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

             					 // Ensure masks and default ICC profiles exist on disk.
                                printJobNocai.prepareNocaiAssets()

                                // Reload ICC profiles and select the default
								iccProfileModel.clear()
								const profiles = printJobNocai.getAvailableICCProfiles()

								for (let i = 0; i < profiles.length; ++i) {
									iccProfileModel.append(profiles[i])
								}

						        // Sync Output ICC dropdown to backend default.
								const currentDefault = printJobNocai.getDefaultOutputICCProfile()

								for (let i = 0; i < iccProfileModel.count; ++i) {
									if (iccProfileModel.get(i).path === currentDefault) {
										Qt.callLater(() => {
											iccProfileDropdown.currentIndex = i
										})
										break
									}
								}
								
								
								// Sync Input CMYK dropdown to backend default.
								const currentInputCmyk = printJobNocai.getDefaultInputCMYKProfile()
								
								for (let i = 0; i < iccProfileModel.count; ++i) {
									if (iccProfileModel.get(i).path === currentInputCmyk) {
										Qt.callLater(() => { inputCmykDropdown.currentIndex = i })
										break
									}
								}
								
								// Sync the toggle state from backend
						        useInputCmykSwitch.checked = printJobNocai.checkDefaultInputCMYK()

                                toast.show("Nocai printer selected: " + selected)
                            }
                        }
                    }
                    
					// Reminder for simulated mode behavior.
                	ColumnLayout {
	                    spacing: 10
	                    Layout.fillWidth: true
	                    Layout.alignment: Qt.AlignHCenter

	                    Label {
	                        text: "** The Nocai engine generates PRN files only **"
							Layout.alignment: Qt.AlignCenter
                    	}
                	}

					// Output ICC selection and upload.
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

							// Output ICC upload button
							Button {
								text: "Upload ICC Profile"
								Layout.alignment: Qt.AlignVCenter
								onClicked: {
									iccDialogTarget = "output"
									iccUploadDialog.open()
								}
							}
						}
						
						// Input CMYK selection, toggle, and upload.
						Label {
							text: "Select Default Input CMYK Profile"
							font.bold: true
							Layout.alignment: Qt.AlignCenter
						}

						RowLayout {
							spacing: 8
							Layout.alignment: Qt.AlignHCenter

							Label {
								text: "Use Default Input CMYK"
								Layout.alignment: Qt.AlignVCenter
							}
							Switch {
								id: useInputCmykSwitch
								checked: printJobNocai.checkDefaultInputCMYK()
								onToggled: printJobNocai.enableDefaultInputCMYK(checked)
							}
						}

						RowLayout {
							spacing: 10
							Layout.alignment: Qt.AlignHCenter

							ComboBox {
								id: inputCmykDropdown
								Layout.alignment: Qt.AlignVCenter
								Layout.preferredWidth: 175
								model: iccProfileModel
								textRole: "name"
								enabled: useInputCmykSwitch.checked

								onCurrentIndexChanged: {
									if (enabled && iccProfileModel.count > 0 && inputCmykDropdown.currentIndex >= 0) {
										const selected = iccProfileModel.get(inputCmykDropdown.currentIndex)
										printJobNocai.setDefaultInputCMYKProfile(selected.path)
									}
								}
							}
							
							// Input CMYK upload button
							Button {
								text: "Upload CMYK Profile"
								Layout.alignment: Qt.AlignVCenter
								enabled: useInputCmykSwitch.checked
								onClicked: {
									iccDialogTarget = "inputCMYK"
									iccUploadDialog.open()
								}
							}
						}


						// Shared file picker for both Output and Input CMYK uploads.
						FileDialog {
							id: iccUploadDialog
							title: "Select ICC Profile"
							nameFilters: ["ICC Profiles (*.icc *.icm)", "All Files (*)"]
							fileMode: FileDialog.OpenFile

							onAccepted: {
								let url = iccUploadDialog.file.toString()
								let path = url.startsWith("file://") ? url.slice(7) : url
								let name = path.split("/").pop()

								// Update backend registry
								iccProfileModel.append({ name: name, path: path })
								printJobNocai.addICCProfile(name, path)

								if (iccDialogTarget === "output") {
									iccProfileDropdown.currentIndex = iccProfileModel.count - 1
									printJobNocai.setDefaultOutputICCProfile(path)
								} else {
									inputCmykDropdown.currentIndex = iccProfileModel.count - 1
									printJobNocai.setDefaultInputCMYKProfile(path)
								}
							}
						}
                	}
            	}
            }


	        // Network printer discovery and selection.
	        Item {
				Layout.fillWidth: true
				Layout.fillHeight: true
	    
	            ColumnLayout {
	                Layout.alignment: Qt.AlignHCenter
	                spacing: 10
	                anchors.fill: parent

					// Choose a discovered printer and load its PPD/capabilities.
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

        // Summary of the chosen printer and its capabilities.
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

        // Finalize or exit without changes.
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
	            onClicked: stackView.pop()
	        }
	    }
	}
	
	// Transient notifications for actions and errors.
	Toast {
	    id: toast
	    parent: Overlay.overlay
	}
}
