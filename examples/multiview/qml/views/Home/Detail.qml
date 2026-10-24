import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CaliburnMultiView 1.0

FocusScope {
    id: root
    required property HomeViewModel viewModel
    objectName: "detailView"
    implicitWidth: 360
    implicitHeight: 300
    ColumnLayout {
        anchors.fill: parent
        spacing: 12
        Label { text: "Detail · 概览"; font.pixelSize: 20 }
        Label {
            objectName: "nameSummary"
            text: root.viewModel ? root.viewModel.name : ""
            font.pixelSize: 22
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        Label {
            objectName: "countSummary"
            text: "共享计数：" + (root.viewModel ? root.viewModel.count : 0)
            font.pixelSize: 28
        }
        Label { text: "临时备注（切换 context 后清空）" }
        TextField { objectName: "localNote"; Layout.fillWidth: true }
        Item { Layout.fillHeight: true }
    }
}
