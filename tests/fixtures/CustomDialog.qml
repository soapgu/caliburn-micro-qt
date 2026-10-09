import QtQuick
import QtQuick.Controls
import DialogTest 1.0
FocusScope {
    id: root
    required property CustomDialogVm viewModel
    implicitWidth: 280
    implicitHeight: 140
    Button {
        anchors.centerIn: parent
        objectName: "customDialogAccept"
        text: "自定义完成"
        focus: true
        onClicked: root.viewModel.finish()
    }
}
