import QtQuick
import QtQuick.Controls
import DialogTest 1.0
FocusScope {
    id: root
    required property CustomDialogViewModel viewModel
    implicitWidth: 280
    implicitHeight: 140
    property var windowAtCompletion
    property string titleAtCompletion
    property color backgroundColorAtCompletion
    Component.onCompleted: {
        windowAtCompletion = Window.window
        titleAtCompletion = Window.window ? Window.window.title : ""
        backgroundColorAtCompletion = Window.window ? Window.window.color : "transparent"
    }
    Button {
        anchors.centerIn: parent
        objectName: "customDialogAccept"
        text: "自定义完成"
        focus: true
        onClicked: root.viewModel.finish()
    }
}
