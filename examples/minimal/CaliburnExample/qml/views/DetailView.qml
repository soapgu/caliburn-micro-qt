import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CaliburnExample 1.0

FocusScope {
    id: root
    required property DetailViewModel viewModel
    focus: true
    KeyNavigation.tab: goBackButton
    KeyNavigation.backtab: goBackButton

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - 32, 400)
        spacing: 20
        Label {
            text: "详情"
            font.pixelSize: 24
            Layout.alignment: Qt.AlignHCenter
        }
        Label {
            objectName: "detailMessageLabel"
            text: root.viewModel ? root.viewModel.message : ""
            font.pixelSize: 28
            Layout.alignment: Qt.AlignHCenter
        }
        Button {
            id: goBackButton
            objectName: "goBack"
            text: "返回首页"
            Layout.alignment: Qt.AlignHCenter
            enabled: root.viewModel !== null && root.viewModel.isActive
                && root.viewModel.parentViewModel !== null
            onClicked: { if (root.viewModel) root.viewModel.goBack() }
            KeyNavigation.priority: KeyNavigation.BeforeItem
            KeyNavigation.tab: root
            KeyNavigation.backtab: root
        }
        Label {
            text: "此页只读展示计数，返回首页继续操作。"
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
        }
    }
}
