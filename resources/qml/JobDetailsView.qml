import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.platform
import QtCore
import "."

// Job details & settings editor for a single print job.
// Loads/validates artwork, shows live preview, and lets the user tweak resolution, offsets, and edit the job image
Item {
    id: root
    required property StackView stackView
    required property int jobIndex
    required property var jobModel
    required property var appState
    required property Theme theme
    property var jobData: jobModel.getJob(jobIndex)
    property string imagePath: jobData.imagePath           // What the preview <Image> points at (may be a temp PNG for PDFs)
    property string tempPreviewPath: ""                    // Temp file created for PDF preview; cleaned on Back
    property var imageMeta: ({})                           // Metadata from ImageLoader
    property string selectedInputICC: ""                   // When using "Custom ICC" conversion
    property string selectedOutputICC: ""
    property string printedSizeDisplay: "Printed size unavailable"
    property bool loadingInputICC: true

    width: parent ? parent.width : 450
    height: parent ? parent.height : 600
	
    // DPI options depend on backend/printer type:
    // - Nocai: Y divisible by 720 (720, 1440, 2160)
    // - MultiInk: Y divisible by 600 (600, 1200, 1800)
    property bool usingMultiInk: appState && appState.usingMultiInkPrinter === true

    property var dpiOptionsNocai:    ["720x720", "720x1440", "720x2160"]
    property var dpiOptionsMultiInk: ["720x600", "720x1200", "720x1800"]
    
	property string whitePlatePath: ""
	property string varnishPlatePath: ""
    
    property var dpiOptions: usingMultiInk ? dpiOptionsMultiInk : dpiOptionsNocai
    
    Rectangle {
		anchors.fill: parent
		color: theme.bg
		z: -1
    }

    // On first show: derive preview path (PDF -> temp PNG), pull metadata, sync DPI widgets, and gate controls against printer caps.
    Component.onCompleted: {
        if (jobData.imagePath !== "") {
            updateMetadata(jobData.imagePath)
            
            if (jobData.imagePath.toLowerCase().endsWith(".pdf")) {
            	const previewPath = imageLoader.renderPdfToPreviewImage(jobData.imagePath)
            	
            	if (previewPath !== "") {
                    imagePath = "file://" + previewPath
                    tempPreviewPath = previewPath
            	} else {
                    console.warn("Failed to regenerate preview for PDF.")
                    imagePath = ""
            	}
            } else {
            	imagePath = jobData.imagePath
            	tempPreviewPath = ""
            }
        }
        
        // Sync DPI dropdown from saved jobData.resolution using correct backend list
        syncResolutionComboToJob()
        updatePrintedSize()
        
        // Sync White/Varnish Plates
        whitePlatePath = jobData.whitePlatePath || ""
		varnishPlatePath = jobData.varnishPlatePath || ""

        if (appState.selectedPrinter.length > 0) {
            safeSelectFirstSupported(profileBox, printJobOutput.supportedColorModes())
            safeSelectFirstSupported(paperSizeBox, printJobOutput.supportedMediaSizes())
            // TODO: Add more fields as neccessary
		}
    }
    
    
    Connections {
        target: appState
        function onUsingMultiInkPrinterChanged() {
            // Rebind dpiOptions (it depends on usingMultiInk) and resync selection
            syncResolutionComboToJob()
            updatePrintedSize()
        }
    }


    // When navigating back to this screen, refresh preview and derived labels.
    onVisibleChanged: {
        if (visible) {
			jobData = jobModel.getJob(jobIndex)
	        whitePlatePath = jobData.whitePlatePath || ""
	        varnishPlatePath = jobData.varnishPlatePath || ""
	        refreshPreview()
	        updatePrintedSize()
	        
	        if (jobData.imagePath !== "") {
				updateMetadata(jobData.imagePath)
			}
        }
    }

	
    // Force the <Image> to reload its source; handy after conversions.
    function refreshPreview() {
        const temp = previewImage.source
        previewImage.source = ""
        previewImage.source = temp
    }

	
    // Capability guard: if no capability list is provided, assume supported.
    function isSupported(value, supportedList) {
        if (!supportedList || supportedList.length === 0)
            return true
        return supportedList.indexOf(value) !== -1
    }

	
    // Pick the first dropdown value that the current printer supports.
    function safeSelectFirstSupported(comboBox, supportedList) {
        if (!supportedList || supportedList.length === 0)
            return
        for (let i = 0; i < comboBox.count; i++) {
            if (supportedList.indexOf(comboBox.model[i]) !== -1) {
                comboBox.currentIndex = i
                break
            }
        }
    }

	
    // Update job's persisted image path.
    function updateImagePath(path) {
        jobData.imagePath = path
    }


    // Map jobData.paperSize to the Paper Size combo index.
    function paperSizeIndexFromSize(size) {
        if (size.width === 210 && size.height === 297) return 0; // A4
        if (size.width === 216 && size.height === 279) return 1; // Letter
        if (size.width === 279 && size.height === 432) return 2; // Tabloid
        return 3; // Custom
    }

	
    // Apply paper size selection back into jobData (handles Custom).
    function updatePaperSize() {
        if (paperSizeBox.currentText === "A4") {
            jobData.paperSize = Qt.size(210, 297)
        }
        else if (paperSizeBox.currentText === "Letter") {
            jobData.paperSize = Qt.size(216, 279)
        }
        else if (paperSizeBox.currentText === "Tabloid") {
            jobData.paperSize = Qt.size(279, 432)
        }
        else if (paperSizeBox.currentText === "Custom") {
            jobData.paperSize = Qt.size(customWidth.value, customHeight.value)
        }
    }

	
	// Parse "WxH" combo text and write to jobData.resolution.
	function updateResolution() {
		let dpiText = resolutionComboBox.currentText
		let parts = dpiText.split("x")
		if (parts.length === 2) {
		    jobData.resolution = Qt.size(parseInt(parts[0]), parseInt(parts[1]))
		}
	}
	
	
	function dpiStringFromSize(sz) {
        return (sz ? (sz.width + "x" + sz.height) : "")
    }


    // If a saved DPI doesn't exist in the active list, try to map it to an equivalent
    function mapDpiBetweenBackends(dpiStr) {
        // Common mappings between 720-based Y and 600-based Y.
        // You can extend this if you add more presets later.
        const mapToMulti = {
            "720x720":  "720x600",
            "720x1440": "720x1200",
            "720x2160": "720x1800"
        }
        const mapToNocai = {
            "720x600":  "720x720",
            "720x1200": "720x1440",
            "720x1800": "720x2160"
        }

        if (usingMultiInk && mapToMulti[dpiStr]) return mapToMulti[dpiStr]
        if (!usingMultiInk && mapToNocai[dpiStr]) return mapToNocai[dpiStr]
        return dpiStr
    }


    function syncResolutionComboToJob() {
        const saved = dpiStringFromSize(jobData.resolution)
        let idx = dpiOptions.indexOf(saved)

        if (idx < 0) {
            const mapped = mapDpiBetweenBackends(saved)
            idx = dpiOptions.indexOf(mapped)
            if (idx >= 0) {
                // Update the job to the mapped value so the UI + saved state stay aligned
                const parts = mapped.split("x")
                if (parts.length === 2) {
                    jobData.resolution = Qt.size(parseInt(parts[0]), parseInt(parts[1]))
                }
            }
        }

        // Fallback defaults per backend
        if (idx < 0) {
            const fallback = usingMultiInk ? "720x1200" : "720x1440"
            idx = dpiOptions.indexOf(fallback)
            const parts = fallback.split("x")
            jobData.resolution = Qt.size(parseInt(parts[0]), parseInt(parts[1]))
        }

        resolutionComboBox.currentIndex = idx
    }


	// Compute approximate printed size using selected X DPI (from the dropdown).
	// (We assume X DPI governs physical size for width; you can also show Y if you want.)
	function updatePrintedSize() {
		const wPx = imageMeta.width || 0
		const hPx = imageMeta.height || 0

		// Physical size uses a fixed base DPI per printer family:
		// - Nocai: 720 baseline
		// - MultiInk: 600 baseline
		const effectiveDpi = usingMultiInk ? 600 : 720

		if (wPx > 0 && hPx > 0 && effectiveDpi > 0) {
		    const printedWmm = ((wPx * 25.4) / effectiveDpi).toFixed(1)
		    const printedHmm = ((hPx * 25.4) / effectiveDpi).toFixed(1)

		    // Still show the selected output DPI so user knows the "detail mode"
		    const dpiText = resolutionComboBox.currentText || ""
		    printedSizeDisplay = `Approx. Printed Size: ${printedWmm} mm × ${printedHmm} mm`
		} else {
		    printedSizeDisplay = "Printed size unavailable"
		}
	}

	
    // Persist offset spinboxes to jobData.
    function updateOffset() {
        jobData.offset = Qt.point(offsetXSpin.value, offsetYSpin.value)
    }


    // Persist white/varnish/profile selections.
    function updateWhiteStrategy() { jobData.whiteStrategy = whiteBox.currentText }
    function updateVarnishType() { jobData.varnishType = varnishBox.currentText }
    function updateColorProfile() { jobData.colorProfile = profileBox.currentText }

	// Update White and Varnish Plate UI after loading in plate image
    function updateWhitePlatePath(path) {
		whitePlatePath = path
		jobData.whitePlatePath = path
	}

	function updateVarnishPlatePath(path) {
		varnishPlatePath = path
		jobData.varnishPlatePath = path
	}
    
    	
    // Fetch fresh metadata (dimensions, channels, color space guess, etc.).
    function updateMetadata(path) {
        imageMeta = imageLoader.extractMetadata(path)
    }

	
    // Main scroller for the form content; reduces flick velocity for desktop feel.
    ColumnLayout {
        width: parent.width
    	height: parent.height
    	
		Rectangle {
			id: headerBar
			Layout.fillWidth: true
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
					    onClicked: {
				        if (tempPreviewPath !== "") {
				            imageLoader.deleteTemporaryFile(tempPreviewPath)
				            tempPreviewPath = ""
				        }
				        stackView.pop()
				    }
				}

				Item { Layout.fillWidth: true }

				Label {
				    text: "Job Details"
				    color: theme.text
				    font.pixelSize: 20
				    font.weight: Font.Medium
				    horizontalAlignment: Text.AlignHCenter
				    verticalAlignment: Text.AlignVCenter
				    Layout.alignment: Qt.AlignVCenter
				}

				Item { Layout.fillWidth: true }

					ThemedButton {
					    text: "Save"
					    theme: root.theme
					    Layout.preferredWidth: 88
					    padding: 12
					    font.pixelSize: 15
					    onClicked: {
				        jobModel.updateJob(jobIndex, {
				            name: jobNameField.text,
				            imagePath: jobData.imagePath,
				            paperSize: jobData.paperSize,
				            resolution: (function() {
				                let parts = resolutionComboBox.currentText.split("x")
				                return (parts.length === 2)
				                    ? Qt.size(parseInt(parts[0]), parseInt(parts[1]))
				                    : Qt.size(720, usingMultiInk ? 1200 : 1440)
				            })(),
				            offset: Qt.point(offsetXSpin.value, offsetYSpin.value),
				            whiteStrategy: whiteBox.currentText,
				            varnishType: varnishBox.currentText,
				            colorProfile: profileBox.currentText,
		                    whitePlatePath: whitePlatePath,
					        varnishPlatePath: varnishPlatePath
				        })
				        toast.show("Job Successfully Saved!")
				    }
				}
			}

			// optional: subtle divider line like a toolbar
			Rectangle {
				anchors.left: parent.left
				anchors.right: parent.right
				anchors.bottom: parent.bottom
				height: 1
				color: theme.divider
				opacity: 0.8
			}
		}

        ScrollView {
        
		id: scrollView
		Layout.alignment: Qt.AlignHCenter
		Layout.fillHeight: true
		Layout.fillWidth: true

		ScrollBar.vertical.interactive: true
		ScrollBar.vertical.policy: ScrollBar.AsNeeded

		// Wait until flickableItem is ready
		Connections {
			target: scrollView
			function onContentItemChanged() {
				if (scrollView.flickableItem) {
					scrollView.flickableItem.flickDeceleration = 500
					scrollView.flickableItem.maximumFlickVelocity = 8000
				}
			}
		}


	    // Card-like container for all job controls.
		Column {
			width: implicitWidth
			Layout.alignment: Qt.AlignHCenter
			spacing: 0

			Pane {
			Layout.alignment: Qt.AlignHCenter
			Layout.minimumWidth: 300
			Layout.preferredWidth: 400
			Layout.maximumWidth: 450
			Layout.topMargin: 12
			padding: 20

			background: Rectangle {
				color: theme.surface
				radius: 12
				border.width: 1
				border.color: theme.divider
			}

                    ColumnLayout {
			id: columnContent
			spacing: 16
			Layout.fillWidth: true
                        Layout.alignment: Qt.AlignHCenter

			// Job name label
			Label {
				text: "Job Name"
			}

			// Job name field
			TextField {
				id: jobNameField
				text: jobData.name
				placeholderText: "Enter Job Name"
				Layout.fillWidth: true
			}

			// Artwork preview area; shows temp PNG for PDFs.
            Rectangle {
				id: imageContainer
			    Layout.fillWidth: true
			    height: 260
				color: theme.surface
			    border.color: theme.divider
			    radius: 10
                Layout.alignment: Qt.AlignHCenter
                clip: true

                Image {
                    id: previewImage
                    anchors.centerIn: parent
                    anchors.margins: 8
                    source: imagePath
                    fillMode: Image.PreserveAspectFit
                    width: parent.width
                    height: parent.height
                    smooth: true
                    visible: source !== ""
                    cache: false
                    clip: true

                    opacity: visible ? 1.0 : 0.0
                    Behavior on opacity { NumberAnimation { duration: 200 } }
                }

				Text {
					anchors.centerIn: parent
					text: jobData.imagePath === "" ? "No image loaded" : ""
					visible: imagePath == ""
					color: theme.subtext
				}
			}

			// Artwork actions: load, open editor, open imposition tool.
            RowLayout {
                spacing: 12
                Layout.alignment: Qt.AlignHCenter

                ThemedButton {
                    text: "Upload Image"
                    theme: root.theme
                    padding: 12
					font.pixelSize: 14	
                    onClicked: imageDialog.open()
                }

                ThemedButton {
                    text: "Edit Image"
                    enabled: imagePath !== ""
                    theme: root.theme
					padding: 12
					font.pixelSize: 14
	                    onClicked: {
	                        stackView.push("qrc:/qml/ImageEditorView.qml", {
	                            "imagePath": imagePath,
	                            "stackView": stackView,
	                            "theme": root.theme
	                        })
	                    }
	                }

                ThemedButton {
                    text: "Edit Imposition"
                    enabled: imagePath !== ""
                    theme: root.theme
                    padding: 12
					font.pixelSize: 14
                    onClicked: {
							stackView.push("qrc:/qml/ImpositionView.qml", {
								"jobIndex": jobIndex,
								"jobModel": jobModel,
								"stackView": stackView,
								"initialImagePath": imagePath,
								"theme": root.theme
							})
	                    }
                }
            }

			// File picker for artwork; validates and builds a PDF preview if needed.
            FileDialog {
                id: imageDialog
                title: "Select Image for Job"
                nameFilters: ["Images (*.png *.jpg *.jpeg *.bmp *.tif *.tiff *.svg *.pdf)"]
                
                onAccepted: {
                	var filePath = String(file)
                	
                    if (imageLoader.isSupportedExtension(file)) {
                        if (imageLoader.validateFile(file)) {
                            updateMetadata(file)
					
							if (filePath.toLowerCase().endsWith(".pdf")) {
								const previewPath = imageLoader.renderPdfToPreviewImage(file)
				
								if (previewPath !== "") {
									imagePath = "file://" + previewPath
									tempPreviewPath = previewPath // Track for later cleanup
								} else {
									console.warn("Failed to render PDF preview.")
								}                			    
							} else {
								imagePath = file
								tempPreviewPath = "" // Clear old preview if switching to non-PDF
							}

								updateImagePath(filePath)
								updatePrintedSize()

                            } else {
                            	console.warn("File validation failed.")
                            }
			    	} else {
						console.warn("Unsupported file type.")
					}
				}
			}
						
			// Printer-related settings: media, DPI, offsets, white/varnish.
            Label {
                text: "Printer Settings"
                font.pixelSize: 18
                horizontalAlignment: Text.AlignHCenter
                Layout.alignment: Qt.AlignHCenter
            }
            
            Rectangle {
				Layout.fillWidth: true
				height: 1
				color: theme.divider
				opacity: 0.8
			}

            GroupBox {
                Layout.fillWidth: true

                ColumnLayout {
                    spacing: 10
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignHCenter

                    Label { text: "Paper Size" }
                    ComboBox {
                        id: paperSizeBox
                        Layout.fillWidth: true
                        model: ["A4", "Letter", "Tabloid", "Custom"]
                        currentIndex: paperSizeIndexFromSize(jobData.paperSize)
                        onCurrentTextChanged: updatePaperSize()

                        enabled: appState.selectedPrinter.length === 0 || isSupported(currentText, printJobOutput.supportedMediaSizes())
                    }
								
				// Custom size widgets appear only when needed.
                ColumnLayout {
                    visible: paperSizeBox.currentText === "Custom"
                    spacing: 8

                    Label { text: "Custom Paper Size (Width × Height, mm)" }

                    RowLayout {
                        spacing: 8
                        Layout.fillWidth: true

                        SpinBox {
                            id: customWidth
                            from: 10; to: 2000
                            value: jobData.paperSize.width
                            editable: true
                            Layout.fillWidth: true
                            onValueChanged: jobData.paperSize.width = value
                        }

                        Label { text: "×" }

                        SpinBox {
                            id: customHeight
                            from: 10; to: 2000
                            value: jobData.paperSize.height
                            editable: true
                            Layout.fillWidth: true
                            onValueChanged: jobData.paperSize.height = value
                        }
                    }
                }


				Label { text: "Output DPI (X × Y)" }								
				RowLayout {
					spacing: 8
					Layout.fillWidth: true

					ComboBox {
						id: resolutionComboBox
						Layout.fillWidth: true
						model: dpiOptions
						currentIndex: -1

						onCurrentIndexChanged: {
							updateResolution()
							updatePrintedSize()
						}
				    }
				}
								
				// Calculated printed size hint for the user.
				Label {
					id: printedSizeLabel
					text: printedSizeDisplay
					font.italic: true
					font.pointSize: 10
					wrapMode: Text.Wrap
				}

                Label { text: "Offset (X × Y)" }
                RowLayout {
                    spacing: 8
                    Layout.fillWidth: true

                    SpinBox {
                        id: offsetXSpin
                        from: 0; to: 10000
                        value: jobData.offset.x
                        editable: true
                        validator: IntValidator { bottom: 0 }
                        Layout.fillWidth: true
                        onValueChanged: updateOffset()
                    }

                    Label { text: "×" }

                    SpinBox {
                        id: offsetYSpin
                        from: 0; to: 10000
                        value: jobData.offset.y
                        editable: true
                        validator: IntValidator { bottom: 0 }
                        Layout.fillWidth: true
                        onValueChanged: updateOffset()
                    }
                }

				Label { text: "White Mode" }
				ComboBox {
					id: whiteBox
					Layout.fillWidth: true
					model: ["Off", "Auto Underbase", "Flood", "Plate"]
					currentIndex: Math.max(0, model.indexOf(jobData.whiteStrategy))
					onCurrentTextChanged: updateWhiteStrategy()
				}
				
				ColumnLayout {
					visible: whiteBox.currentText === "Plate"
					Layout.fillWidth: true
					spacing: 8

					Label {
						text: "White Plate File"
						color: theme.text
					}

					RowLayout {
						Layout.fillWidth: true
						spacing: 8

						TextField {
							Layout.fillWidth: true
							text: whitePlatePath
							readOnly: true
							placeholderText: "No white plate selected"
						}

						ThemedButton {
							text: "Browse"
							theme: root.theme
							onClicked: whitePlateDialog.open()
						}
					}
				}
				
				Text {
					visible: whiteBox.currentText === "Auto Underbase"
					text: "Uses the printer mode defaults from Color Management."
					color: theme.subtext
					wrapMode: Text.Wrap
					font.pixelSize: 12
				}
				
				FileDialog {
					id: whitePlateDialog
					title: "Select White Plate"
					nameFilters: ["Images (*.png *.jpg *.jpeg *.bmp *.tif *.tiff)"]
					fileMode: FileDialog.OpenFile

					onAccepted: {
						updateWhitePlatePath(String(file))
					}
				}

				Label { text: "Varnish Mode" }
				ComboBox {
					id: varnishBox
					Layout.fillWidth: true
					model: ["Off", "Over Printed Area", "Flood", "Plate"]
					currentIndex: Math.max(0, model.indexOf(jobData.varnishType))
					onCurrentTextChanged: updateVarnishType()
				}
				
				ColumnLayout {
					visible: varnishBox.currentText === "Plate"
					Layout.fillWidth: true
					spacing: 8

					Label {
						text: "Varnish Plate File"
						color: theme.text
					}

					RowLayout {
						Layout.fillWidth: true
						spacing: 8

						TextField {
							Layout.fillWidth: true
							text: varnishPlatePath
							readOnly: true
							placeholderText: "No varnish plate selected"
						}

						ThemedButton {
							text: "Browse"
							theme: root.theme
							onClicked: varnishPlateDialog.open()
						}
					}
				}
				
				Text {
					visible: varnishBox.currentText === "Over Printed Area"
					text: "Uses the printer mode defaults from Color Management."
					color: theme.subtext
					wrapMode: Text.Wrap
					font.pixelSize: 12
				}
				
				FileDialog {
					id: varnishPlateDialog
					title: "Select Varnish Plate"
					nameFilters: ["Images (*.png *.jpg *.jpeg *.bmp *.tif *.tiff)"]
					fileMode: FileDialog.OpenFile

					onAccepted: {
						updateVarnishPlatePath(String(file))
					}
				}
                                
				// Color space selection and optional ICC-driven conversion.
                Label { text: "Color Profile" }
                ComboBox {
                    id: profileBox
                    Layout.fillWidth: true
                    model: ["sRGB", "AdobeRGB", "CMYK", "Lc+Lm+Ly+Lk", "Grayscale", "Indexed8", "Indexed16", "Custom ICC"]
                    currentIndex: model.indexOf(jobData.colorProfile)
                    enabled: appState.selectedPrinter.length === 0 || isSupported(currentText, printJobOutput.supportedColorModes())
                }

				// Action row for profile-driven conversions.
                RowLayout {
                    spacing: 8
                    Layout.fillWidth: true

                    ThemedButton {
						visible: profileBox.currentText === "Custom ICC"
						text: "Load Input ICC"
						theme: root.theme
						onClicked: { loadingInputICC = true; iccDialog.open() }
				    }

				    ThemedButton {
						visible: profileBox.currentText === "Custom ICC"
						text: "Load Output ICC"
						theme: root.theme
						onClicked: { loadingInputICC = false; iccDialog.open() }
				    }

				    ThemedButton {
						id: convertButton
						text: "Convert Colorspace"
						theme: root.theme
						visible: profileBox.currentText !== jobData.colorProfile
						enabled: imagePath !== ""

                        onClicked: {
                            var result = false
                            if (profileBox.currentText === "Custom ICC") {
                                result = colorProfile.convertWithICCProfilesCMYK(imagePath, imagePath, selectedInputICC, selectedOutputICC)
                            } else {
                                result = colorProfile.convertToColorspace(imagePath, profileBox.currentText)
                            }

                            if (result) {
                                updateColorProfile()
                                refreshPreview()
                                toast.show("Color space converted!")
                            } else {
                                toast.show("Color space conversion failed.")
                            }
                        }
                    }
                }

				// Show selected ICCs to show user what ICC will be applied.
                Text {
                    text: "Input: " + selectedInputICC
                    visible: profileBox.currentText === "Custom ICC" && selectedInputICC !== ""
    				color: theme.subtext
                    wrapMode: Text.Wrap
                    font.pixelSize: 12
                }

                Text {
                    text: "Output: " + selectedOutputICC
                    visible: profileBox.currentText === "Custom ICC" && selectedOutputICC !== ""
    				color: theme.subtext
                    wrapMode: Text.Wrap
                    font.pixelSize: 12
                }

					// Shared ICC picker; writes to input or output depending on the toggle.
		            FileDialog {
		                id: iccDialog
		                title: "Select ICC Profile"
		                nameFilters: ["ICC Profiles (*.icc *.icm)"]
		                onAccepted: {
		                    if (loadingInputICC) {
		                        selectedInputICC = file
		                      //  colorProfile.setInputICC(file) // This only applies when using LittleCMS, we use Image Magick instead
		                    } else {
		                        selectedOutputICC = file
		                      //  colorProfile.setOutputICC(file) // This only applies when using LittleCMS, we use Image Magick instead
		                    }
		                }
		            }
		        }
			}
                        
                        Rectangle {
							Layout.fillWidth: true
							height: 1
							color: theme.divider
							opacity: 0.8
						}

						// Raw metadata dump (only shows keys that exist).
                        GroupBox {
                            title: "Image Metadata"
                            Layout.fillWidth: true
                            visible: Object.keys(imageMeta).length > 0

                            Column {
                                spacing: 4

                                // Always shown if present
                                Text { text: "Name: " + imageMeta.name; color: theme.text; visible: imageMeta.name !== undefined }
                                Text { text: "Size: " + imageMeta.size + " bytes"; color: theme.text; visible: imageMeta.size !== undefined }
                                Text { text: "Dimensions: " + imageMeta.width + " x " + imageMeta.height; color: theme.text; visible: imageMeta.width !== undefined && imageMeta.height !== undefined }
                                Text { text: "Channels: " + imageMeta.channels; color: theme.text; visible: imageMeta.channels !== undefined }
                                Text { text: "Format: " + imageMeta.format; color: theme.text; visible: imageMeta.format !== undefined }
                                Text { text: "DPI: " + imageMeta.dpi; color: theme.text; visible: imageMeta.dpi !== undefined }
                                Text { text: "Color Profile: " + imageMeta.colorProfile; color: theme.text; visible: imageMeta.colorProfile !== undefined }

                                // SVG
                                Text { text: "SVG Size: " + imageMeta.svgWidth + " x " + imageMeta.svgHeight; color: theme.text; visible: imageMeta.svgWidth !== undefined && imageMeta.svgHeight !== undefined }
                                Text { text: "SVG Title: " + imageMeta.svgTitle; color: theme.text; visible: imageMeta.svgTitle !== undefined }

                                // PDF
                                Text { text: "PDF Version: " + imageMeta.pdfVersion; color: theme.text; visible: imageMeta.pdfVersion !== undefined }
                                Text { text: "PDF Title: " + imageMeta.pdfTitle; color: theme.text; visible: imageMeta.pdfTitle !== undefined }
                                Text { text: "Page Count: " + imageMeta.pageCount; color: theme.text; visible: imageMeta.pageCount !== undefined }
                            }
                        }
                    }
                }
            }
        }

        Toast {
            id: toast
            parent: Overlay.overlay
        }
    }
}
