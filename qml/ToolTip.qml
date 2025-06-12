// ToolTip.qml
import QtQuick
import QtQuick.Controls

ToolTip {
    padding: 6
    background: Rectangle {
        color: "#2a82da"
        border.color: "white"
        border.width: 1
        radius: 4
    }

    contentItem: Text {
        text: control.text
        color: "white"
        font.pixelSize: 14
        wrapMode: Text.Wrap
    }
}

