import QtQuick

Item {
    id: root

    property Component sourceComponent
    property real itemX: 0      // In mm
    property real itemY: 0      // In mm
    property real zoomFactor: 1.0
    property alias contentItem: contentLoader.item

    // Internal width/height track original content size (unscaled)
    width: loaderWrapper.implicitWidth
    height: loaderWrapper.implicitHeight

    // Position based on logical coordinates
    x: itemX * zoomFactor
    y: itemY * zoomFactor

    Item {
        id: loaderWrapper
        anchors.fill: parent

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
