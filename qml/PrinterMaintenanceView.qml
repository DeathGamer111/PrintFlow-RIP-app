import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.platform as P
import "."

Page {
    id: root

    required property var stackView
    required property var appState
    required property Theme theme

    readonly property bool maintenanceSupported: nocaiDirectPrint.supportsMaintenance(appState.selectedPrinter)
    readonly property bool controlsEnabled: maintenanceSupported && nocaiDirectPrint.available
    property bool statusPollingEnabled: false

    property int headMask: 3
    property int axis: 0
    property int axisDirection: 0
    property int printHeight: 0
    property int printX: 0
    property int printY: 0
    property int alignmentType: 0
    property int alignmentPatternType: 0
    property int uvType: 0
    property int newUvType: 0
    property int newUvFunctionType: 0
    property string statusText: nocaiDirectPrint.statusText()
    property var printerStatus: ({})
    property var jobSettings: ({})
    property var alignmentValues: ({})
    property var uvValues: ({})
    property var newUvValues: ({})

    background: Rectangle {
        color: root.theme.bg
    }

    component Section: Pane {
        id: section
        property Theme theme: root.theme
        property string title: ""
        property string help: ""
        property bool sectionEnabled: true
        default property alias content: body.data

        Layout.fillWidth: true
        padding: 14
        opacity: sectionEnabled ? 1.0 : 0.46

        background: Rectangle {
            color: section.theme.surface
            radius: 8
            border.width: 1
            border.color: section.theme.divider
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 10

            Label {
                text: section.title
                color: section.theme.text
                font.pixelSize: 17
                font.weight: Font.Medium
                Layout.fillWidth: true
            }

            Label {
                visible: section.help.length > 0
                text: section.help
                color: section.theme.subtext
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            Rectangle {
                height: 1
                color: section.theme.divider
                opacity: 0.75
                Layout.fillWidth: true
            }

            ColumnLayout {
                id: body
                Layout.fillWidth: true
                spacing: 10
                enabled: section.sectionEnabled
            }
        }
    }

    component ActionButton: ThemedButton {
        theme: root.theme
        Layout.fillWidth: true
        Layout.preferredHeight: 42
        padding: 10
        font.pixelSize: 13
    }

    component FieldLabel: Label {
        property Theme theme: root.theme
        color: theme.text
        verticalAlignment: Text.AlignVCenter
        Layout.fillWidth: true
    }

    function printStatusText(code) {
        switch (Number(code)) {
        case 0:
            return "Standby";
        case 1:
            return "Printing";
        case 2:
            return "Paused";
        case 3:
            return "Resume";
        case 4:
            return "Canceled";
        case 5:
            return "Error";
        default:
            return "Unknown";
        }
    }

    function cleanStatusText(code) {
        switch (Number(code)) {
        case 0:
            return "Standby";
        case 1:
            return "Auto-cleaning";
        default:
            return "Unknown";
        }
    }

    function readMapValue(map, key, fallback) {
        if (!map || map[key] === undefined)
            return fallback;
        return map[key];
    }

    function updateStatus(silent) {
        if (!root.controlsEnabled)
            return false;
        const result = nocaiDirectPrint.getPrinterStatus();
        root.printerStatus = result;
        if (result.ok) {
            root.statusText = "Print: " + root.printStatusText(result.printStatus) + " | Clean: " + root.cleanStatusText(result.cleanStatus);
            return true;
        }
        root.statusText = nocaiDirectPrint.lastError;
        if (!silent)
            toast.show(root.statusText);
        return false;
    }

    function runAction(label, callback, pollAfter) {
        if (!root.maintenanceSupported) {
            root.statusText = "Maintenance is not supported for " + appState.selectedPrinter + ".";
            toast.show(root.statusText);
            return false;
        }
        const ok = callback();
        root.statusText = ok ? label + " succeeded." : label + " failed: " + nocaiDirectPrint.lastError;
        toast.show(root.statusText);
        if (pollAfter)
            Qt.callLater(function () {
                    if (root.statusPollingEnabled)
                        root.updateStatus(true);
                });
        return ok;
    }

    function refreshJobSettings() {
        const result = nocaiDirectPrint.getJobSettings();
        jobSettings = result;
        runAction("GetJobSettings", function () {
                return result.ok;
            }, false);
    }

    function refreshAlignment() {
        const result = nocaiDirectPrint.getAlignmentValues();
        alignmentValues = result;
        runAction("GetAlignmentValues", function () {
                return result.ok;
            }, false);
    }

    function refreshUv() {
        const result = nocaiDirectPrint.getUVParamValues();
        uvValues = result;
        runAction("GetUVParamValues", function () {
                return result.ok;
            }, false);
    }

    function refreshNewUv() {
        const result = nocaiDirectPrint.getNewUVParamValues();
        newUvValues = result;
        runAction("GetNewUVParamValues", function () {
                return result.ok;
            }, false);
    }

    Timer {
        id: statusPoller
        interval: 1500
        repeat: true
        running: root.visible && root.controlsEnabled && root.statusPollingEnabled
        onTriggered: root.updateStatus(true)
    }

    header: Rectangle {
        height: 60
        color: root.theme.surface

        RowLayout {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 10

            ThemedButton {
                text: "Back"
                theme: root.theme
                Layout.preferredWidth: 88
                Layout.preferredHeight: 40
                onClicked: root.stackView.pop()
            }

            Label {
                text: "Printer Maintenance"
                color: root.theme.text
                font.pixelSize: 20
                font.weight: Font.Medium
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
            }

            ThemedButton {
                text: "Connect"
                theme: root.theme
                enabled: root.maintenanceSupported
                Layout.preferredWidth: 88
                Layout.preferredHeight: 40
                onClicked: root.runAction("ConnectPrinter", function () {
                        if (root.appState.sdkSelectedPrinterIndex >= 0)
                            nocaiDirectPrint.choosePrinter(root.appState.sdkSelectedPrinterIndex);
                        const ok = nocaiDirectPrint.connectPrinter();
                        root.statusPollingEnabled = ok;
                        if (ok)
                            Qt.callLater(root.updateStatus, true);
                        return ok;
                    }, false)
            }
        }
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
            width: Math.min(parent.width, 760)
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 12
            anchors.margins: 12

            Section {
                title: "Status"
                theme: root.theme
                sectionEnabled: root.maintenanceSupported
                help: "The SDK recommends polling printer status slower than once per second. This page refreshes print and cleaning status every 1.5 seconds while maintenance is available."

                Label {
                    Layout.fillWidth: true
                    text: root.maintenanceSupported ? root.statusText : "Maintenance is unavailable for the selected printer. Select a supported printer type in Printer Setup first."
                    color: root.maintenanceSupported ? root.theme.subtext : root.theme.warning
                    wrapMode: Text.WordWrap
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Label {
                        Layout.fillWidth: true
                        color: root.theme.text
                        text: "Print: " + root.printStatusText(root.readMapValue(root.printerStatus, "printStatus", -1))
                    }

                    Label {
                        Layout.fillWidth: true
                        color: root.theme.text
                        text: "Clean: " + root.cleanStatusText(root.readMapValue(root.printerStatus, "cleanStatus", -1))
                    }
                }

                ActionButton {
                    text: "Refresh Status"
                    onClicked: {
                        root.statusPollingEnabled = true;
                        root.updateStatus(false);
                    }
                }
            }

            Section {
                title: "Head Maintenance"
                theme: root.theme
                sectionEnabled: root.controlsEnabled
                help: "Head mask is a bitmask: bit 0 selects head 1, bit 1 selects head 2, and so on. A value of 3 selects heads 1 and 2."

                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: 12
                    rowSpacing: 8

                    FieldLabel {
                        text: "Head Mask"
                        theme: root.theme
                    }
                    SpinBox {
                        from: 0
                        to: 65535
                        value: root.headMask
                        onValueModified: root.headMask = value
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: 10
                    rowSpacing: 10

                    ActionButton {
                        text: "Wipe Heads"
                        theme: root.theme
                        onClicked: root.runAction("WipePrintHead", function () {
                                return nocaiDirectPrint.wipePrintHead(root.headMask);
                            }, true)
                    }
                    ActionButton {
                        text: "Auto Clean"
                        theme: root.theme
                        onClicked: root.runAction("StartCleanOperation", function () {
                                return nocaiDirectPrint.startCleanOperation(root.headMask);
                            }, true)
                    }
                    ActionButton {
                        text: "Start Pump"
                        theme: root.theme
                        onClicked: root.runAction("StartPump", function () {
                                return nocaiDirectPrint.startPump(root.headMask);
                            }, true)
                    }
                    ActionButton {
                        text: "Stop Pump"
                        theme: root.theme
                        onClicked: root.runAction("StopPumpOperation", function () {
                                return nocaiDirectPrint.stopPumpOperation();
                            }, true)
                    }
                    ActionButton {
                        text: "Spit Heads"
                        theme: root.theme
                        onClicked: root.runAction("SpitPrintHead", function () {
                                return nocaiDirectPrint.spitPrintHead(root.headMask);
                            }, true)
                    }
                    ActionButton {
                        text: "Stop Spit"
                        theme: root.theme
                        onClicked: root.runAction("StopSpitOperation", function () {
                                return nocaiDirectPrint.stopSpitOperation();
                            }, true)
                    }
                    ActionButton {
                        text: "Cap Head"
                        theme: root.theme
                        onClicked: root.runAction("CapPrintHead", function () {
                                return nocaiDirectPrint.capPrintHead();
                            }, true)
                    }
                }
            }

            Section {
                title: "Motion And Height"
                theme: root.theme
                sectionEnabled: root.controlsEnabled
                help: "Axis 0 is X, 1 is Y, and 2 is Z. Direction 0 moves positive; direction 1 moves negative. The SDK reports saved/stop positions in millimeters where supported."

                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: 12
                    rowSpacing: 8

                    FieldLabel {
                        text: "Axis"
                        theme: root.theme
                    }
                    SpinBox {
                        from: 0
                        to: 2
                        value: root.axis
                        onValueModified: root.axis = value
                    }
                    FieldLabel {
                        text: "Direction"
                        theme: root.theme
                    }
                    SpinBox {
                        from: 0
                        to: 1
                        value: root.axisDirection
                        onValueModified: root.axisDirection = value
                    }
                    FieldLabel {
                        text: "Print Height mm"
                        theme: root.theme
                    }
                    SpinBox {
                        from: 0
                        to: 65535
                        value: root.printHeight
                        onValueModified: root.printHeight = value
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: 10
                    rowSpacing: 10

                    ActionButton {
                        text: "Move Axis"
                        theme: root.theme
                        onClicked: root.runAction("MoveAxis", function () {
                                return nocaiDirectPrint.moveAxis(root.axis, root.axisDirection);
                            }, true)
                    }
                    ActionButton {
                        text: "Stop Axis"
                        theme: root.theme
                        onClicked: root.runAction("StopAxis", function () {
                                const r = nocaiDirectPrint.stopAxis(root.axis);
                                if (r.ok)
                                    root.statusText = "StopAxis position: " + r.position + " mm";
                                return r.ok;
                            }, true)
                    }
                    ActionButton {
                        text: "Save Axis Position"
                        theme: root.theme
                        onClicked: root.runAction("SaveAxisPos", function () {
                                const r = nocaiDirectPrint.saveAxisPos(root.axis);
                                if (r.ok)
                                    root.statusText = "Saved position: " + r.position + " mm";
                                return r.ok;
                            }, true)
                    }
                    ActionButton {
                        text: "Set Height"
                        theme: root.theme
                        onClicked: root.runAction("SetPrintHeight", function () {
                                return nocaiDirectPrint.setPrintHeight(root.printHeight);
                            }, true)
                    }
                    ActionButton {
                        text: "Get Height"
                        theme: root.theme
                        onClicked: root.runAction("GetPrintHeight", function () {
                                const r = nocaiDirectPrint.getPrintHeight();
                                root.printHeight = r.heightMm;
                                if (r.ok)
                                    root.statusText = "Print height: " + r.heightMm + " mm";
                                return r.ok;
                            }, false)
                    }
                }
            }

            Section {
                title: "Settings And Config"
                theme: root.theme
                sectionEnabled: root.controlsEnabled
                help: "Read the engine's current job settings before applying changes. Import/export uses the vendor PFG configuration file format."

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    ActionButton {
                        text: "Read Job Settings"
                        theme: root.theme
                        onClicked: root.refreshJobSettings()
                    }
                    ActionButton {
                        text: "Apply Read Settings"
                        theme: root.theme
                        onClicked: root.runAction("SetJobSettings", function () {
                                return nocaiDirectPrint.setJobSettingsFromMap(root.jobSettings);
                            }, true)
                    }
                }

                Label {
                    Layout.fillWidth: true
                    text: "Direction " + root.readMapValue(root.jobSettings, "printDirection", "-") + " | Speed " + root.readMapValue(root.jobSettings, "printSpeed", "-") + " | Head Voltage " + root.readMapValue(root.jobSettings, "headVoltage", "-")
                    color: root.theme.subtext
                    wrapMode: Text.WordWrap
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    ActionButton {
                        text: "Export Config"
                        theme: root.theme
                        onClicked: exportConfigDialog.open()
                    }
                    ActionButton {
                        text: "Import Config"
                        theme: root.theme
                        onClicked: importConfigDialog.open()
                    }
                }
            }

            Section {
                title: "Alignment"
                theme: root.theme
                sectionEnabled: root.controlsEnabled
                help: "Value type selects which alignment field to write. Pattern type selects the printer-generated alignment chart, including nozzle check, step, bidirectional, spacing, and channel-alignment patterns."

                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: 12
                    rowSpacing: 8

                    FieldLabel {
                        text: "Value Type"
                        theme: root.theme
                    }
                    SpinBox {
                        from: 0
                        to: 5
                        value: root.alignmentType
                        onValueModified: root.alignmentType = value
                    }
                    FieldLabel {
                        text: "Pattern Type"
                        theme: root.theme
                    }
                    SpinBox {
                        from: 0
                        to: 22
                        value: root.alignmentPatternType
                        onValueModified: root.alignmentPatternType = value
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    ActionButton {
                        text: "Read Alignment"
                        theme: root.theme
                        onClicked: root.refreshAlignment()
                    }
                    ActionButton {
                        text: "Apply Alignment"
                        theme: root.theme
                        onClicked: root.runAction("SetAlignmentValues", function () {
                                return nocaiDirectPrint.setAlignmentValues(root.alignmentValues, root.alignmentType);
                            }, true)
                    }
                }

                ActionButton {
                    text: "Print Alignment Pattern"
                    theme: root.theme
                    onClicked: root.runAction("PrintAlignmentPattern", function () {
                            return nocaiDirectPrint.printAlignmentPattern(root.alignmentPatternType);
                        }, true)
                }

                Label {
                    Layout.fillWidth: true
                    text: "Step " + root.readMapValue(root.alignmentValues, "stepValue", "-") + " | Bidi " + root.readMapValue(root.alignmentValues, "bidiValue", "-")
                    color: root.theme.subtext
                    wrapMode: Text.WordWrap
                }
            }

            Section {
                title: "XY And UV"
                theme: root.theme
                sectionEnabled: root.controlsEnabled
                help: "XY offsets are in millimeters. UV value types map to the right/left lamp directional offsets documented by the SDK. New UV controls are only active on firmware that reports support."

                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: 12
                    rowSpacing: 8

                    FieldLabel {
                        text: "X Offset mm"
                        theme: root.theme
                    }
                    SpinBox {
                        from: 0
                        to: 65535
                        value: root.printX
                        onValueModified: root.printX = value
                    }
                    FieldLabel {
                        text: "Y Offset mm"
                        theme: root.theme
                    }
                    SpinBox {
                        from: 0
                        to: 65535
                        value: root.printY
                        onValueModified: root.printY = value
                    }
                    FieldLabel {
                        text: "UV Value Type"
                        theme: root.theme
                    }
                    SpinBox {
                        from: 0
                        to: 4
                        value: root.uvType
                        onValueModified: root.uvType = value
                    }
                    FieldLabel {
                        text: "New UV Value Type"
                        theme: root.theme
                    }
                    SpinBox {
                        from: 0
                        to: 6
                        value: root.newUvType
                        onValueModified: root.newUvType = value
                    }
                    FieldLabel {
                        text: "New UV Function"
                        theme: root.theme
                    }
                    SpinBox {
                        from: 0
                        to: 8
                        value: root.newUvFunctionType
                        onValueModified: root.newUvFunctionType = value
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: 10
                    rowSpacing: 10

                    ActionButton {
                        text: "Set XY"
                        theme: root.theme
                        onClicked: root.runAction("SetPrintXYValue", function () {
                                return nocaiDirectPrint.setPrintXYValue(root.printX, root.printY);
                            }, true)
                    }
                    ActionButton {
                        text: "Get XY"
                        theme: root.theme
                        onClicked: root.runAction("GetPrintXYValue", function () {
                                const r = nocaiDirectPrint.getPrintXYValue();
                                root.printX = r.xMm;
                                root.printY = r.yMm;
                                if (r.ok)
                                    root.statusText = "XY: " + r.xMm + ", " + r.yMm + " mm";
                                return r.ok;
                            }, false)
                    }
                    ActionButton {
                        text: "Read UV"
                        theme: root.theme
                        onClicked: root.refreshUv()
                    }
                    ActionButton {
                        text: "Apply UV"
                        theme: root.theme
                        onClicked: root.runAction("SetUVParamValues", function () {
                                return nocaiDirectPrint.setUVParamValues(root.uvValues, root.uvType);
                            }, true)
                    }
                    ActionButton {
                        text: "New UV Support"
                        theme: root.theme
                        onClicked: root.runAction("GetSupportNewUVParamFunction", function () {
                                const support = nocaiDirectPrint.getSupportNewUVParamFunction();
                                root.statusText = "New UV support: " + support;
                                return support >= 0;
                            }, false)
                    }
                    ActionButton {
                        text: "Run New UV Function"
                        theme: root.theme
                        onClicked: root.runAction("SetNewUVParamFunction", function () {
                                return nocaiDirectPrint.setNewUVParamFunction(root.newUvFunctionType);
                            }, true)
                    }
                    ActionButton {
                        text: "Read New UV"
                        theme: root.theme
                        onClicked: root.refreshNewUv()
                    }
                    ActionButton {
                        text: "Apply New UV"
                        theme: root.theme
                        onClicked: root.runAction("SetNewUVParamValues", function () {
                                return nocaiDirectPrint.setNewUVParamValues(root.newUvValues, root.newUvType);
                            }, true)
                    }
                }
            }

            Item {
                height: 8
            }
        }
    }

    P.FileDialog {
        id: exportConfigDialog
        title: "Export Nocai Config"
        fileMode: P.FileDialog.SaveFile
        defaultSuffix: "pfg"
        nameFilters: ["Printer Config (*.pfg)", "All Files (*)"]
        onAccepted: root.runAction("ExportConfigFile", function () {
                return nocaiDirectPrint.exportConfigFile(file);
            }, true)
    }

    P.FileDialog {
        id: importConfigDialog
        title: "Import Nocai Config"
        fileMode: P.FileDialog.OpenFile
        nameFilters: ["Printer Config (*.pfg)", "All Files (*)"]
        onAccepted: root.runAction("ImportConfigFile", function () {
                return nocaiDirectPrint.importConfigFile(file);
            }, true)
    }

    Toast {
        id: toast
        parent: Overlay.overlay
    }
}
