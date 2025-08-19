import QtQuick


/* DraggableItem.qml
 * 	Reusable draggable wrapper that positions its content in logical (mm) space,
 * 	scaled by a provided zoomFactor. Updates itemX/itemY after drag to persist
 * 	logical coordinates. 
 */
Item {

    id: root

	// Content to render inside the draggable wrapper
    property Component sourceComponent
    
    // Logical coordinates in millimeters; UI position is x/y = itemX/Y * zoomFactor
    property real itemX: 0      // In mm
    property real itemY: 0      // In mm
    
    // Scale factor for screen rendering (1.0 = 100% size)
    property real zoomFactor: 1.0
    
    // Convenience alias to the instantiated child item
    property alias contentItem: contentLoader.item

	// Track the contents intrinsic size (prior to scaling)
    width: loaderWrapper.implicitWidth
    height: loaderWrapper.implicitHeight

	// Derive screen-space position from logical mm coordinates
    x: itemX * zoomFactor
    y: itemY * zoomFactor


	// Loader host for the provided component
    Item {
        id: loaderWrapper
        anchors.fill: parent
		
		// The actual content; when loaded, capture intrinsic size if available
        Loader {
            id: contentLoader
            anchors.fill: parent
            sourceComponent: root.sourceComponent
            
			onLoaded: {
                console.log("Loader item loaded:", item)
                console.log("    → width:", item?.width, "implicitWidth:", item?.implicitWidth,
                            "height:", item?.height, "implicitHeight:", item?.implicitHeight)
                if (item && item.implicitWidth > 0)
                    loaderWrapper.implicitWidth = item.implicitWidth
                if (item && item.implicitHeight > 0)
                    loaderWrapper.implicitHeight = item.implicitHeight
            }
        }
		
		// Mouse-based dragging; updates logical coordinates after release
        MouseArea {
            id: dragArea
            anchors.fill: parent
            onPressed: console.log("Mouse pressed on draggable")
            drag.target: root

            onReleased: {
                // Update logical coordinates after drag
                root.itemX = root.x / root.zoomFactor
                root.itemY = root.y / root.zoomFactor
                console.log("Drag released. New logical position:", root.itemX, root.itemY)
            }
        }

		// Touch dragging (single-finger pan); keeps itemX/itemY in sync while moving
        MultiPointTouchArea {
            anchors.fill: parent
            touchPoints: [ TouchPoint { id: touch } ]
            onTouchUpdated: {
                root.x += (touch.x - touch.previousX)
                root.y += (touch.y - touch.previousY)

                // Sync logical position
                root.itemX = root.x / root.zoomFactor
                root.itemY = root.y / root.zoomFactor
                console.log("Touch drag. Updated itemX/Y:", root.itemX, root.itemY)
            }
        }
    }
}
