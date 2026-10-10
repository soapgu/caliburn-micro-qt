import QtQuick
import Caliburn.Micro.Qt 1.0

Window {
    id: root
    objectName: "dialogWindow"
    flags: Qt.Dialog
    modality: Qt.ApplicationModal
    visible: false
    title: qsTr("对话框")
    color: palette.window
    property alias model: view.model
    readonly property Item dialogItem: view.item
    readonly property string errorString: view.errorString
    readonly property real preferredWidth: (view.item && view.item.implicitWidth > 0
                                           ? view.item.implicitWidth : 360) + 40
    readonly property real preferredHeight: (view.item && view.item.implicitHeight > 0
                                            ? view.item.implicitHeight : 220) + 40
    signal dismissRequested()

    Shortcut {
        sequence: "Esc"
        context: Qt.WindowShortcut
        onActivated: root.dismissRequested()
    }

    ViewHost {
        id: view
        anchors.fill: parent
        anchors.margins: 20
        focus: true
    }
}
