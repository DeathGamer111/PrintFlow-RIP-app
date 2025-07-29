import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.platform
import QtCore


Item {
    required property StackView stackView
    required property int jobIndex
    required property var jobModel
    property var jobData: jobModel.getJob(jobIndex)
    property string imagePath: jobData.imagePath
    property string tempPreviewPath: ""
    property var imageMeta: ({})
    property var appState
    property string selectedInputICC: ""
    property string selectedOutputICC: ""
    property string printedSizeDisplay: "Printed size unavailable"
    property var dpiOptions: ["720x720", "720x1440", "720x2160"]
    property bool loadingInputICC: true

	width: parent ? parent.width : 450
	height: parent ? parent.height : 600

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
        
        // Set saved DPI
		const savedDPI = jobData.resolution.width + "x" + jobData.resolution.height
		const dpiIdx = dpiOptions.indexOf(savedDPI)
		resolutionComboBox.currentIndex = dpiIdx >= 0 ? dpiIdx : dpiOptions.indexOf("720x720")
		updatePrintedSize()

        if (appState.selectedPrinter.length > 0) {
            safeSelectFirstSupported(profileBox, printJobOutput.supportedColorModes())
            safeSelectFirstSupported(paperSizeBox, printJobOutput.supportedMediaSizes())
            // TODO: Add more fields as neccessary
		}
    }

    onVisibleChanged: {
        if (visible) {
			jobData = jobModel.getJob(jobIndex)
	        refreshPreview()
	        updatePrintedSize()
        }
    }

    function refreshPreview() {
        const temp = previewImage.source
        previewImage.source = ""
        previewImage.source = temp
    }

    function isSupported(value, supportedList) {
        if (!supportedList || supportedList.length === 0)
            return true
        return supportedList.indexOf(value) !== -1
    }

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

    function updateImagePath(path) {
        jobData.imagePath = path
    }

    function paperSizeIndexFromSize(size) {
        if (size.width === 210 && size.height === 297) return 0; // A4
        if (size.width === 216 && size.height === 279) return 1; // Letter
        if (size.width === 279 && size.height === 432) return 2; // Tabloid
        return 3; // Custom
    }

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

	function updateResolution() {
		let dpiText = resolutionComboBox.currentText
		let parts = dpiText.split("x")
		if (parts.length === 2) {
		    jobData.resolution = Qt.size(parseInt(parts[0]), parseInt(parts[1]))
		}
	}
	
	function updatePrintedSize() {
		const dpi = 720
		const w = imageMeta.width || 0
		const h = imageMeta.height || 0

		if (w > 0 && h > 0) {
		    const printedWmm = ((w * 25.4) / dpi).toFixed(1)
		    const printedHmm = ((h * 25.4) / dpi).toFixed(1)
		    printedSizeDisplay = `Approx. Printed Size: ${printedWmm} mm × ${printedHmm} mm at 720 DPI`
		} else {
		    printedSizeDisplay = "Printed size unavailable"
		}
	}

    function updateOffset() {
        jobData.offset = Qt.point(offsetXSpin.value, offsetYSpin.value)
    }

    function updateWhiteStrategy() {
        jobData.whiteStrategy = whiteBox.currentText
    }

    function updateVarnishType() {
        jobData.varnishType = varnishBox.currentText
    }

    function updateColorProfile() {
        jobData.colorProfile = profileBox.currentText
    }
    
    function updateDotStrategy() {
		jobData.minInkThreshold = minInkSpin.value
		jobData.smallDotThreshold = smallDotSpin.value
		jobData.medDotThreshold = medDotSpin.value
		jobData.enablePromotion = promotionCheck.checked
	}

    function updateMetadata(path) {
        imageMeta = imageLoader.extractMetadata(path)
    }

    ColumnLayout {
        width: parent.width
    	height: parent.height

        ScrollView {
	        id: scrollView
			Layout.alignment: Qt.AlignHCenter
			Layout.fillHeight: true

            ScrollBar.vertical.interactive: true
            ScrollBar.vertical.policy: ScrollBar.AsNeeded

			// Wait until flickableItem is ready
			Connections {
				target: scrollView
				function onContentItemChanged() {
					if (scrollView.flickableItem) {
						scrollView.flickableItem.flickDeceleration = 500		// default is 3000
						scrollView.flickableItem.maximumFlickVelocity = 8000 	// default is 2500
					}
				}
			}

            Column {
            	width: implicitWidth
				Layout.alignment: Qt.AlignHCenter
				spacing: 0

                Pane {
					Layout.alignment: Qt.AlignHCenter
					Layout.minimumWidth: 300
					Layout.preferredWidth: 400
					Layout.maximumWidth: 450
					padding: 20

                    ColumnLayout {
						id: columnContent
						spacing: 16
						Layout.fillWidth: true
                        Layout.alignment: Qt.AlignHCenter

                        TextField {
                            id: jobNameField
                            text: jobData.name
                            placeholderText: "Enter Job Name"
                            Layout.fillWidth: true
                        }

                        Rectangle {
                            id: imageContainer
                            width: 300
                            height: 200
                            color: "#eeeeee"
                            border.color: "#999"
                            Layout.alignment: Qt.AlignHCenter
                            clip: true

                            Image {
                                id: previewImage
                                anchors.centerIn: parent
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
                            }
                        }

                        RowLayout {
                            spacing: 12
                            Layout.alignment: Qt.AlignHCenter

                            Button {
                                text: "Upload Image"
                                onClicked: imageDialog.open()
                            }

                            Button {
                                text: "Edit Image"
                                enabled: imagePath !== ""
                                onClicked: {
                                    stackView.push("qrc:/qml/ImageEditorView.qml", { "imagePath": imagePath })
                                }
                            }

                            Button {
                                text: "Edit Imposition"
                                enabled: imagePath !== ""
                                onClicked: {
									stackView.push("qrc:/qml/ImpositionView.qml", {
										"jobIndex": jobIndex,
										"jobModel": jobModel
									})
                                }
                            }
                        }

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

                        Label {
                            text: "Printer Settings"
                            font.pixelSize: 18
                            horizontalAlignment: Text.AlignHCenter
                            Layout.alignment: Qt.AlignHCenter
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
										currentIndex: {
											const dpiString = jobData.resolution.width + "x" + jobData.resolution.height
											const idx = dpiOptions.indexOf(dpiString)
											return idx >= 0 ? idx : dpiOptions.indexOf("720x720")
										}
										onCurrentIndexChanged: updateResolution()
									}
                                }
								
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

                                Label { text: "White Strategy" }
                                ComboBox {
                                    id: whiteBox
                                    Layout.fillWidth: true
                                    model: ["None", "Underprint", "Overprint", "Flood", "Spot", "Knockout"]
                                    currentIndex: model.indexOf(jobData.whiteStrategy)
                                    onCurrentTextChanged: updateWhiteStrategy()
                                }

                                Label { text: "Varnish Layer" }
                                ComboBox {
                                    id: varnishBox
                                    Layout.fillWidth: true
                                    model: ["None", "Spot", "Full"]
                                    currentIndex: model.indexOf(jobData.varnishType)
                                    onCurrentTextChanged: updateVarnishType()
                                }
                                
                                Label { text: "Ink Dot Strategy" }
								GroupBox {
									Layout.fillWidth: true
									
									ColumnLayout {
										spacing: 10

										RowLayout {
											spacing: 8
											Label { text: "Min Ink Threshold"; Layout.alignment: Qt.AlignLeft }
											SpinBox {
												id: minInkSpin
												from: 0; to: 255
												value: jobData.minInkThreshold
												onValueChanged: updateDotStrategy()
												Layout.fillWidth: true
											}
										}

										RowLayout {
											spacing: 8
											Label { text: "Small Dot Threshold"; Layout.alignment: Qt.AlignLeft }
											SpinBox {
												id: smallDotSpin
												from: 0; to: 255
												value: jobData.smallDotThreshold
												onValueChanged: updateDotStrategy()
												Layout.fillWidth: true
											}
										}

										RowLayout {
											spacing: 8
											Label { text: "Medium Dot Threshold"; Layout.alignment: Qt.AlignLeft }
											SpinBox {
												id: medDotSpin
												from: 0; to: 255
												value: jobData.medDotThreshold
												onValueChanged: updateDotStrategy()
												Layout.fillWidth: true
											}
										}

										CheckBox {
											id: promotionCheck
											text: "Enable Dot Promotion"
											checked: jobData.enablePromotion
											onCheckedChanged: updateDotStrategy()
										}
									}
								}


                                Label { text: "Color Profile" }
                                ComboBox {
                                    id: profileBox
                                    Layout.fillWidth: true
                                    model: ["sRGB", "AdobeRGB", "CMYK", "Lc+Lm+Ly+Lk", "Grayscale", "Indexed8", "Indexed16", "Custom ICC"]
                                    currentIndex: model.indexOf(jobData.colorProfile)
                                    enabled: appState.selectedPrinter.length === 0 || isSupported(currentText, printJobOutput.supportedColorModes())
                                }

                                RowLayout {
                                    spacing: 8
                                    Layout.fillWidth: true

                                    Button {
                                        visible: profileBox.currentText === "Custom ICC"
                                        text: "Load Input ICC"
                                        onClicked: {
                                            loadingInputICC = true
                                            iccDialog.open()
                                        }
                                    }

                                    Button {
                                        visible: profileBox.currentText === "Custom ICC"
                                        text: "Load Output ICC"
                                        onClicked: {
                                            loadingInputICC = false
                                            iccDialog.open()
                                        }
                                    }

                                    Button {
                                        id: convertButton
                                        text: "Convert Colorspace"
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

                                Text {
                                    text: "Input: " + selectedInputICC
                                    visible: profileBox.currentText === "Custom ICC" && selectedInputICC !== ""
                                    color: "lightgray"
                                    wrapMode: Text.Wrap
                                    font.pixelSize: 12
                                }

                                Text {
                                    text: "Output: " + selectedOutputICC
                                    visible: profileBox.currentText === "Custom ICC" && selectedOutputICC !== ""
                                    color: "lightgray"
                                    wrapMode: Text.Wrap
                                    font.pixelSize: 12
                                }

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

                        GroupBox {
                            title: "Image Metadata"
                            Layout.fillWidth: true
                            visible: Object.keys(imageMeta).length > 0

                            Column {
                                spacing: 4

                                // Always shown if present
                                Text { text: "Name: " + imageMeta.name; color: "white"; visible: imageMeta.name !== undefined }
                                Text { text: "Size: " + imageMeta.size + " bytes"; color: "white"; visible: imageMeta.size !== undefined }
                                Text { text: "Dimensions: " + imageMeta.width + " x " + imageMeta.height; color: "white"; visible: imageMeta.width !== undefined && imageMeta.height !== undefined }
                                Text { text: "Channels: " + imageMeta.channels; color: "white"; visible: imageMeta.channels !== undefined }
                                Text { text: "Format: " + imageMeta.format; color: "white"; visible: imageMeta.format !== undefined }
                                Text { text: "DPI: " + imageMeta.dpi; color: "white"; visible: imageMeta.dpi !== undefined }
                                Text { text: "Color Profile: " + imageMeta.colorProfile; color: "white"; visible: imageMeta.colorProfile !== undefined }

                                // SVG
                                Text { text: "SVG Size: " + imageMeta.svgWidth + " x " + imageMeta.svgHeight; color: "white"; visible: imageMeta.svgWidth !== undefined && imageMeta.svgHeight !== undefined }
                                Text { text: "SVG Title: " + imageMeta.svgTitle; color: "white"; visible: imageMeta.svgTitle !== undefined }

                                // PDF
                                Text { text: "PDF Version: " + imageMeta.pdfVersion; color: "white"; visible: imageMeta.pdfVersion !== undefined }
                                Text { text: "PDF Title: " + imageMeta.pdfTitle; color: "white"; visible: imageMeta.pdfTitle !== undefined }
                                Text { text: "Page Count: " + imageMeta.pageCount; color: "white"; visible: imageMeta.pageCount !== undefined }
                            }
                        }
                    }
                }
            }
        }

        Pane {
            Layout.fillWidth: true
            padding: 10
            Layout.alignment: Qt.AlignHCenter

            RowLayout {
				anchors.horizontalCenter: parent.horizontalCenter
				spacing: 20

                Button {
                    text: "Save Job"
                    onClicked: {
                        jobModel.updateJob(jobIndex, {
                            name: jobNameField.text,
                            imagePath: jobData.imagePath,
                            paperSize: jobData.paperSize,
                            resolution: (function() {
								let parts = resolutionComboBox.currentText.split("x")
								return (parts.length === 2) ? Qt.size(parseInt(parts[0]), parseInt(parts[1])) : Qt.size(720, 720)
							})(),
                            offset: Qt.point(offsetXSpin.value, offsetYSpin.value),
                            whiteStrategy: whiteBox.currentText,
                            varnishType: varnishBox.currentText,
                            colorProfile: profileBox.currentText,
                            minInkThreshold: minInkSpin.value,
							smallDotThreshold: smallDotSpin.value,
							medDotThreshold: medDotSpin.value,
							enablePromotion: promotionCheck.checked
                        })
                        toast.show("Job Successfully Saved!")
                    }
                }

                Button {
                    text: "Back"
                    onClicked: {
                    	if (tempPreviewPath !== "") {
							imageLoader.deleteTemporaryFile(tempPreviewPath)
			        		tempPreviewPath = ""
		        		}
                    	stackView.pop()
                    }
                }

                Toast {
                    id: toast
                    parent: Overlay.overlay
                }
            }
        }
    }
}

