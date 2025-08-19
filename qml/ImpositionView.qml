import QtQuick
import QtQuick.Controls
import QtQuick.Layouts


/* ImpositionView.qml
 * Canvas-style editor to position a job image on a physical page (paperSize in mm)
 * and optionally burn simple overlays (text/rectangle) into a new imposed image.
 * Coordinates persist as mm offsets; zoom only affects on-screen scale.
 */
Page {
    id: impositionView
    title: "Imposition Editor"

	// Model hooks
	required property int jobIndex
	required property var jobModel
	
	
	// Function to pull in current job model
	function jobData() {
		return jobModel.getJob(jobIndex)
	}
	
	
	// Source + media state (paper size in mm)
	property string imagePath: jobData().imagePath
	property size paperSize: jobData().paperSize
	
	
	// Overlay state
    property bool hasDrawnElements: false


	// Zoom (1.0 = 100%) auto-fit based on paper size and viewport box
    property real zoomFactor: 1.0
	onWidthChanged: updateZoom()
	onHeightChanged: updateZoom()

	function updateZoom() {
		if (paperSize.width > 0 && paperSize.height > 0 && impositionBox.width > 0 && impositionBox.height > 0) {
			zoomFactor = Math.min(
				impositionBox.width / paperSize.width,
				impositionBox.height / paperSize.height
			)
		}
	}
	
	
	// Build an updated job payload; optionally swap imagePath when overlays are baked in
    function cloneJobWithOffset(newOffset, newPath = null) {
		return {
		    name: jobData().name,
		    imagePath: newPath || jobData().imagePath,
		    paperSize: jobData().paperSize,
		    resolution: jobData().resolution,
		    offset: newOffset,
		    whiteStrategy: jobData().whiteStrategy,
		    varnishType: jobData().varnishType,
		    colorProfile: jobData().colorProfile
		}
	}


	// Overlay command dispatcher for live preview (draws when saving via C++ backend)
	function apply(type, value) {
		const actions = {
		    "text":     () => imageEditor.drawText(value.text, value.x, value.y),
		    "drawRect": () => imageEditor.drawRectangle(value.x, value.y, value.w, value.h)
		}

		if (actions[type]) {
		    actions[type]()
		    hasDrawnElements = true
		} else {
		    console.warn("Unknown draw action:", type)
		}
	}


	// Main Layout
    ColumnLayout {
        anchors.fill: parent
        spacing: 8
		anchors.margins: 5
        
        // Zoom toolbar
        RowLayout {
			Layout.alignment: Qt.AlignCenter
            spacing: 12
			Layout.margins: 5

            Button {
                text: "-"
                onClicked: zoomFactor = Math.max(zoomFactor - 0.1, 0.05)
            }

            Label {
                text: Math.round(zoomFactor * 100) + "%"
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            Button {
                text: "+"
                onClicked: zoomFactor = Math.min(zoomFactor + 0.1, 5.0)
            }
        }

		// Virtual Paper Size View; inner content is measured in mm, scaled by zoomFactor
        Rectangle {
            id: impositionBox
			Layout.fillWidth: true
			Layout.preferredHeight: 400
			Layout.maximumHeight: 400
			Layout.minimumHeight: 200
            color: "#f0f0f0"
            border.color: "black"
            border.width: 1
            clip: true

			// Paper plane (mm) centered in the viewport
			Item {
				id: paperArea
				width: paperSize.width
				height: paperSize.height
				scale: zoomFactor
				anchors.centerIn: parent
				clip: true

				Rectangle {
					anchors.fill: parent
					color: "white"
					border.color: "black"
					border.width: 1
				}

				// Draggable job image proxy; offset stored in mm (x,y)
                Item {
                    id: imageWrapper
					property real itemX: (jobModel.getJob(jobIndex).offset?.x !== undefined) ? jobModel.getJob(jobIndex).offset.x : 0
					property real itemY: (jobModel.getJob(jobIndex).offset?.y !== undefined) ? jobModel.getJob(jobIndex).offset.y : 0

                    x: itemX * zoomFactor
                    y: itemY * zoomFactor

                    width: imageItem.width
                    height: imageItem.height

					// Image is sized from pixels → mm using assumed 720 DPI (25.4 mm/in)
                    Image {
                        id: imageItem
                        source: jobData().imagePath
                        fillMode: Image.PreserveAspectFit
                        asynchronous: true
                        visible: status === Image.Ready

                        readonly property real dpi: 720
                        readonly property real pxWidth: jobData().imageWidth || implicitWidth || 1000
                        readonly property real pxHeight: jobData().imageHeight || implicitHeight || 1000

                        width: (pxWidth / dpi) * 25.4
                        height: (pxHeight / dpi) * 25.4

                        onStatusChanged: {
                            if (status === Image.Ready) {
                                console.log("Image ready:", source)
                                console.log("Size:", width, height)
                            } else if (status === Image.Error) {
                                console.error("Failed to load image:", source)
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            drag.target: imageWrapper
                            onPressed: console.log("Pressed image for drag")
                            onReleased: {
                                imageWrapper.itemX = imageWrapper.x / zoomFactor
                                imageWrapper.itemY = imageWrapper.y / zoomFactor
                                console.log("Image dragged to:", imageWrapper.itemX, imageWrapper.itemY)
                            }
                        }
                    }
                }
                
				// Draggable text overlay preview (baked at Save)
				Item {
					id: textOverlayWrapper
					property real itemX: 100
					property real itemY: 100

					x: itemX * zoomFactor
					y: itemY * zoomFactor

					width: textItem.width
					height: textItem.height

					visible: textOverlayField.text.length > 0

					Text {
						id: textItem
						text: textOverlayField.text
						font.pointSize: 14
						color: "blue"
					}

					MouseArea {
						anchors.fill: parent
						drag.target: textOverlayWrapper
						onPressed: console.log("Pressed text for drag")
						onReleased: {
							textOverlayWrapper.itemX = textOverlayWrapper.x / zoomFactor
							textOverlayWrapper.itemY = textOverlayWrapper.y / zoomFactor
							console.log("Text dragged to:", textOverlayWrapper.itemX, textOverlayWrapper.itemY)
						}
					}
				}

				// Draggable rectangle overlay preview (baked at Save)
				Item {
					id: rectOverlayWrapper
					property real itemX: 50
					property real itemY: 50

					x: itemX * zoomFactor
					y: itemY * zoomFactor

					width: 100
					height: 100
					
					visible: false

					Rectangle {
						anchors.fill: parent
						color: "transparent"
						border.color: "red"
						border.width: 2
					}

					MouseArea {
						anchors.fill: parent
						drag.target: rectOverlayWrapper
						onPressed: console.log("Pressed rect for drag")
						onReleased: {
							rectOverlayWrapper.itemX = rectOverlayWrapper.x / zoomFactor
							rectOverlayWrapper.itemY = rectOverlayWrapper.y / zoomFactor
							console.log("Rect dragged to:", rectOverlayWrapper.itemX, rectOverlayWrapper.itemY)
						}
					}
				}
			}
		}

		// Overlay drawing controls
		GroupBox {
			title: "Overlay Tools"
			Layout.fillWidth: true
			Layout.alignment: Qt.AlignHCenter

			ColumnLayout {
				anchors.horizontalCenter: parent.horizontalCenter
				spacing: 10

				RowLayout {
				    spacing: 10

				    TextField {
				        id: textOverlayField
				        placeholderText: "Enter text to draw"
				        Layout.preferredWidth: 200
				    }

				    Button {
				        text: "Draw Text"
						onClicked: {
							textOverlayWrapper.visible = true
							apply("text", {
								text: textOverlayField.text,
								x: textOverlayWrapper.itemX,
								y: textOverlayWrapper.itemY
							})
						}
				    }
				}

				RowLayout {
					spacing: 10

					Label { text: "W:" }
					SpinBox {
						id: rectWidthField
						from: 10; to: 500; value: 100; stepSize: 5
						Layout.preferredWidth: 70
					}

					Label { text: "H:" }
					SpinBox {
						id: rectHeightField
						from: 10; to: 500; value: 100; stepSize: 5
						Layout.preferredWidth: 70
					}

					Button {
						text: "Draw Rect"
						onClicked: {
							rectOverlayWrapper.visible = true
							rectOverlayWrapper.width = rectWidthField.value
							rectOverlayWrapper.height = rectHeightField.value
							apply("drawRect", {
								x: rectOverlayWrapper.itemX,
								y: rectOverlayWrapper.itemY,
								w: rectWidthField.value,
								h: rectHeightField.value
							})
						}
					}
				}
			}
		}

		
		// Footer actions: Back or Save (Save persists offset; optionally bakes overlays)
        RowLayout {
            Layout.alignment: Qt.AlignCenter
            spacing: 20
			Layout.margins: 5
            
            Button {
                text: "Back"
                onClicked: stackView.pop()
            }

            Button {
                text: "Save"
                onClicked: {
                    let newOffset = Qt.point(
                        Math.round(imageWrapper.itemX),
                        Math.round(imageWrapper.itemY)
                    )

                    if (hasDrawnElements) {
						imageEditor.applyImpositionEdits(
						    jobData().imagePath,
						    newOffset.x,
						    newOffset.y,
						    paperSize,
						    {
						        text: textOverlayField.text,
						        textX: Math.round(textOverlayWrapper.itemX),
						        textY: Math.round(textOverlayWrapper.itemY),
						        drawRect: rectOverlayWrapper.visible,
						        rectX: Math.round(rectOverlayWrapper.itemX),
						        rectY: Math.round(rectOverlayWrapper.itemY),
						        rectW: Math.round(rectOverlayWrapper.width),
						        rectH: Math.round(rectOverlayWrapper.height)
						    }
						)

                        let imposedPath = jobData().imagePath.replace(/(\.\w+)$/, "_imposed$1")

						jobModel.updateJob(jobIndex, cloneJobWithOffset(newOffset, imposedPath))

                        toast.show("Image updated with drawn elements.")
                    } else {
						jobModel.updateJob(jobIndex, cloneJobWithOffset(newOffset))
                        console.log("Image offset updated:", newOffset)
                        toast.show("Offset position updated.")
                    }
                }
            }

            Toast {
                id: toast
                parent: Overlay.overlay
            }
        }
    }
}
