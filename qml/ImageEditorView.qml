import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.platform

Page {
    id: editorPage
    required property string imagePath
    
    property string tempPath: {
		let parts = imagePath.split(".")
		let ext = parts.length > 1 ? parts[parts.length - 1] : "png"
		return imagePath + ".edit_tmp." + ext
    }

    property bool isDirty: true
    property string currentTool: "none"

    property real cropX: 50
    property real cropY: 50
    property real cropW: 100
    property real cropH: 100

    property real brightness: 0
    property real contrast: 0
    property real hue: 0
    property real saturation: 0
    property real sharpness: 0
    property real gamma: 0
    
    function refreshImageSize() {
	    resizeWidthSpin.value = imageEditor.getImageWidth()
	    resizeHeightSpin.value = imageEditor.getImageHeight()
	}

    Component.onCompleted: {
        if (imageEditor.loadImage(imagePath)) {
            imageEditor.saveImage(tempPath)
            
            if (imageEditor.loadImage(tempPath)) {
                refreshImage()
				refreshImageSize()
            } else {
                console.warn("Failed to load temp image for editing")
            }
        } else {
            console.warn("Failed to load image for editing")
        }
    }

    // === Main Layout ===
    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        // === Top: Image Preview ===
        Pane {
            Layout.fillWidth: true
            Layout.preferredHeight: 260

            Rectangle {
                id: imageContainer
                anchors.fill: parent
                color: "#eeeeee"
                border.color: "#999"
                clip: true

                Image {
                    id: imagePreview
                    source: tempPath
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    cache: false
                    anchors.fill: parent
                }

                Rectangle {
                    x: cropX
                    y: cropY
                    width: cropW
                    height: cropH
                    color: "transparent"
                    border.color: "red"
                    border.width: 2
                    visible: currentTool === "crop"
                }
            }
        }

        // === Middle: Scrollable Tool Panel ===
        ScrollView {
            id: scrollView
            Layout.fillWidth: true
            Layout.fillHeight: true
            ScrollBar.vertical.interactive: true
            ScrollBar.vertical.policy: ScrollBar.AsNeeded

            Component.onCompleted: {
                contentItem.flickableDirection = Flickable.VerticalFlick
            }

            Column {
                width: parent.width
                spacing: 0
                anchors.horizontalCenter: parent.horizontalCenter

                Pane {
                    width: Math.min(parent.width, 450)
                    padding: 20

                    ColumnLayout {
                        id: toolColumn
                        width: parent.width
                        spacing: 20

                        // === View Tools ===
                        GroupBox {
                            title: "View"
                            Layout.fillWidth: true

							ColumnLayout {
								spacing: 12
								Layout.fillWidth: true
								anchors.horizontalCenter: parent.horizontalCenter

								RowLayout {
									spacing: 10
									Layout.fillWidth: true
									Layout.alignment: Qt.AlignHCenter

									Button { 
										text: "Original Size"
										onClicked: {
											apply("resizeOriginal")
											refreshImageSize()
										} 
									}
									Button {
										text: "Half Size"
										onClicked: {
											apply("resizeHalf") 
											refreshImageSize()
										}
									}
									Button {
										text: "Double Size"
										onClicked: {
											apply("resizeDouble")
											refreshImageSize()
										}
									}
								}

								Label { 
									text: "Resize (Width × Height)" 
									Layout.alignment: Qt.AlignHCenter
								}

								RowLayout {
									spacing: 8
									Layout.fillWidth: true
									Layout.alignment: Qt.AlignHCenter

									SpinBox {
										id: resizeWidthSpin
										from: 1; to: 10000
										value: 100
										editable: true
										validator: IntValidator { bottom: 1 }
									}

									Label { text: "×" }

									SpinBox {
										id: resizeHeightSpin
										from: 1; to: 10000
										value: 100
										editable: true
										validator: IntValidator { bottom: 1 }
									}

									Button {
										text: "Resize"
										onClicked: apply("resize", {
											x: resizeWidthSpin.value,
											y: resizeHeightSpin.value
										})
									}
								}
							}
						}

                        // === Transform Tools ===
                        GroupBox {
                            title: "Transform"
                            Layout.fillWidth: true
                            GridLayout {
                                columns: 4
                                anchors.horizontalCenter: parent.horizontalCenter

                                Button { text: "Crop"; onClicked: apply("crop", { x: cropX, y: cropY, w: cropW, h: cropH }) }
                                Button { text: "Flip H"; onClicked: apply("flip", "horizontal") }
                                Button { text: "Flip V"; onClicked: apply("flip", "vertical") }
                                Button { text: "Rotate"; onClicked: apply("rotate", 90) }
                            }
                        }

                        // === Enhance Tools ===
                        GroupBox {
                            title: "Enhance"
                            Layout.fillWidth: true

                            ColumnLayout {
                                anchors.horizontalCenter: parent.horizontalCenter

                                RowLayout {
                                    spacing: 10
                                    Label { text: "Brightness" }
                                    Slider {
                                        id: brightnessSlider
                                        from: -100; to: 100
                                        value: brightness
                                        Layout.fillWidth: true
                                        onValueChanged: {
                                            brightness = value
                                            apply("brightness", { brightness })
                                        }
                                    }
                                    Label { text: brightness.toFixed(0) }
                                }

                                RowLayout {
                                    spacing: 10
                                    Label { text: "Contrast" }
                                    Slider {
                                        id: contrastSlider
                                        from: 0
										to: 100
										value: 50
										stepSize: 1
                                        Layout.fillWidth: true

										onValueChanged: {
											let contrastAmount = Math.abs(value - 50) / 2.0;  // range: 0 → 25
											let increase = value >= 50;

											contrast = contrastAmount;

											if (contrastAmount > 0.1) {
												apply("contrast", { increase, contrast: contrastAmount });
											}
										}
									}
									Label { text: contrast.toFixed(1) }
                                }

                                RowLayout {
                                    spacing: 10
                                    Label { text: "Hue" }
                                    Slider {
                                        id: hueSlider
                                        value: hue
                                        from: -180; to: 180
                                        Layout.fillWidth: true
                                        onValueChanged: {
                                            hue = value
                                            apply("hue", hue)
                                        }
                                    }
                                    Label { text: hue.toFixed(0) }
                                }

                                RowLayout {
                                    spacing: 10
                                    Label { text: "Saturation" }
                                    Slider {
                                        id: saturationSlider
                                        value: saturation
                                        from: 0; to: 200
                                        Layout.fillWidth: true
                                        onValueChanged: {
                                            saturation = value
                                            apply("saturation", saturation)
                                        }
                                    }
                                    Label { text: saturation.toFixed(0) }
                                }

                                RowLayout {
                                    spacing: 10
                                    Label { text: "Sharpen" }
                                    Slider {
                                        id: sharpenSlider
                                        value: sharpness
                                        from: 0; to: 10
                                        Layout.fillWidth: true
                                        onValueChanged: {
                                            sharpness = value
                                            apply("sharpen", sharpness)
                                        }
                                    }
                                    Label { text: sharpness.toFixed(0) }
                                }

                                RowLayout {
                                    spacing: 10
                                    Label { text: "Gamma" }
                                    Slider {
                                        id: gammaSlider
                                        value: gamma
                                        from: 0; to: 5.0
                                        stepSize: 0.1
                                        Layout.fillWidth: true
                                        onValueChanged: {
                                            gamma = value
                                            apply("gamma", gamma)
                                        }
                                    }
                                    Label { text: gamma.toFixed(0) }
                                }
                            }
                        }

                        // === Effects Tools ===
                        GroupBox {
                            title: "Effects"
                            Layout.fillWidth: true

                            GridLayout {
                                columns: 4
                                anchors.horizontalCenter: parent.horizontalCenter

                                Button { text: "Blur"; onClicked: apply("blur", 1.0) }
                                Button { text: "Sepia"; onClicked: apply("sepia") }
                                Button { text: "Vignette"; onClicked: apply("vignette") }
                                Button { text: "Swirl"; onClicked: apply("swirl", 90) }
                                Button { text: "Implode"; onClicked: apply("implode", 0.5) }
                            }
                        }
                    }
                }
            }
        }

        // === Bottom: Save / Back Toolbar ===
        Rectangle {
            Layout.fillWidth: true
            height: 60
            color: "transparent"

            RowLayout {
                anchors.centerIn: parent
                spacing: 20

                Button {
                    text: "Save"
                    onClicked: {
                        if (imageEditor.saveImage(imagePath)) {
                            isDirty = false
                            toast.show("Image saved.")
                        }
                    }
                }

                Button {
                    text: "Undo"
                    onClicked: {
                        if (imageEditor.undo()) {
                            isDirty = true
							imageEditor.saveImage(tempPath)
							refreshImage()
							refreshImageSize()
							resetSliders()
                        }
                    }
                }

                Button {
                    text: "Redo"
                    onClicked: {
                        if (imageEditor.redo()) {
                            isDirty = true
							imageEditor.saveImage(tempPath)
							refreshImage()
							refreshImageSize()
							resetSliders()
                        }
                    }
                }

                Button {
                    text: "Back"
                    // ToolTip.text: "Return to the job list while discarding changes."
                    // ToolTip.visible: hovered
                    onClicked: {
                        if (isDirty) {
                            toast.show("Unsaved changes. Press back again to discard?")
                            isDirty = false
                        } else {
                            cleanupAndExit()
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

    function refreshImage() {
        imagePreview.source = tempPath + "?" + Date.now()
    }

    function cleanupAndExit() {
        imageEditor.deleteFile(tempPath)
        imageEditor.clearUndoRedoStacks()
        stackView.pop()
    }
    
    function resetSliders() {
		brightnessSlider.value = brightness
		contrastSlider.value = contrast
		hueSlider.value = hue
		saturationSlider.value = saturation
		sharpenSlider.value = sharpness
		gammaSlider.value = gamma
	}

    function apply(type, value) {
        const actions = {
            "flip":                 () => imageEditor.flip(value),
            "rotate":               () => imageEditor.rotate(value),
            "brightnessContrast":   () => imageEditor.adjustBrightnessContrast(value.brightness, value.contrast),
            "brightness":			() => imageEditor.adjustBrightness(value.brightness),
            "contrast": 			() => imageEditor.adjustContrast(value.increase, value.contrast, 128.0),
            "hue":                  () => imageEditor.adjustHue(value),
            "saturation":           () => imageEditor.adjustSaturation(value),
            "gamma":                () => imageEditor.adjustGamma(value),
            "sharpen":              () => imageEditor.sharpenImage(value),
            "blur":                 () => imageEditor.applyBlur(value),
            "sepia":                () => imageEditor.applySepia(),
            "vignette":             () => imageEditor.applyVignette(),
            "swirl":                () => imageEditor.applySwirl(value),
            "implode":              () => imageEditor.applyImplode(value),
            "resizeOriginal":       () => imageEditor.resizeToOriginal(),
            "resizeHalf":           () => imageEditor.resizeToHalf(),
            "resizeDouble":         () => imageEditor.resizeToDouble(),
            "resize":				() => imageEditor.resizeImage(value.x, value.y),
            "crop":                 () => imageEditor.crop(value.x, value.y, value.w, value.h),
        }

        let ok = false

        if (actions[type]) {
            try {
                ok = actions[type]()
                isDirty = true
                imageEditor.saveImage(tempPath)
                refreshImage()
            }
            catch (err) {
                console.warn("Error executing action:", type, err)
            }
        }
        else {
            console.warn("Unknown operation:", type)
        }
    }
}
