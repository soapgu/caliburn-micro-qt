import QtQuick
import QtQuick.Controls
import CaliburnMultiView 1.0

FocusScope {
    id: root
    required property HomeViewModel viewModel
    objectName: "defaultView"
    implicitWidth: 360
    implicitHeight: 300
    Label {
        anchors.centerIn: parent
        text: "默认 View\n" + (root.viewModel ? root.viewModel.name : "")
        horizontalAlignment: Text.AlignHCenter
        font.pixelSize: 20
    }
}
