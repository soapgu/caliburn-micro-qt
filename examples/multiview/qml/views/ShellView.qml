import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Caliburn.Micro.Qt 1.0
import CaliburnMultiView 1.0

ApplicationWindow {
    id: root
    required property ShellViewModel viewModel
    width: 880
    height: 540
    minimumWidth: 700
    minimumHeight: 460
    visible: true
    title: "Multi-View：同一个 VM，不同的 View"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 16
        Label { text: "一个 HomeViewModel · 两种界面"; font.pixelSize: 24 }
        Label {
            text: "左侧编辑共享数据，右侧同步显示。切换右侧 context 会重建 View，临时备注随之清空。"
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
        }
        RowLayout {
            Label { text: "右侧 context：" }
            ComboBox {
                id: contextPicker
                objectName: "contextPicker"
                model: ["Master", "Detail", "默认"]
                currentIndex: 1
            }
            Item { Layout.fillWidth: true }
            Button { text: "退出"; onClicked: root.viewModel.exitApplication() }
        }
        RowLayout {
            spacing: 24
            Layout.fillWidth: true
            Layout.fillHeight: true
            ViewHost {
                objectName: "masterHost"
                model: root.viewModel ? root.viewModel.home : null
                context: "Master"
                Layout.fillWidth: true
                Layout.fillHeight: true
            }
            Rectangle { color: "#d9dee7"; Layout.preferredWidth: 1; Layout.fillHeight: true }
            ViewHost {
                objectName: "contextHost"
                model: root.viewModel ? root.viewModel.home : null
                context: contextPicker.currentIndex === 2 ? null : contextPicker.currentText
                Layout.fillWidth: true
                Layout.fillHeight: true
            }
        }
    }
}
