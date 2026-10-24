import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CaliburnMultiView 1.0

FocusScope {
    id: root
    required property HomeViewModel viewModel
    objectName: "masterView"
    implicitWidth: 360
    implicitHeight: 300
    ColumnLayout {
        anchors.fill: parent
        spacing: 12
        Label { text: "Master · 编辑"; font.pixelSize: 20 }
        Label { text: "共享名称（存放在 VM 中）" }
        TextField {
            objectName: "sharedName"
            text: root.viewModel ? root.viewModel.name : ""
            onTextEdited: { if (root.viewModel) root.viewModel.name = text }
            Layout.fillWidth: true
        }
        Button {
            objectName: "increment"
            text: "共享计数 +1（当前 " + (root.viewModel ? root.viewModel.count : 0) + "）"
            onClicked: { if (root.viewModel) root.viewModel.increment() }
        }
        Label { text: "临时备注（只存放在此 View 中）" }
        TextField { objectName: "localNote"; placeholderText: "切换此 View 后清空"; Layout.fillWidth: true }
        Item { Layout.fillHeight: true }
    }
}
