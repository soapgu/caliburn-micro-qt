import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CaliburnExample 1.0

FocusScope {
    id: root
    required property DetailViewModel viewModel
    focus: true

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
        Label {
            text: "此页只读展示计数，返回首页继续操作。"
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
        }
    }
}
