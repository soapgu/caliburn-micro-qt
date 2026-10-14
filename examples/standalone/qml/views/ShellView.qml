import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Caliburn.Micro.Qt 1.0
import CaliburnConsumer 1.0

ApplicationWindow {
    id: root
    objectName: "consumerShell"
    required property ShellViewModel viewModel
    width: 480
    height: 360
    title: "Caliburn.Micro.Qt 独立消费工程"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        Label {
            text: "仅引用框架，不依赖内置示例、Boost.DI 或 spdlog。"
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        ViewHost {
            objectName: "homeHost"
            model: root.viewModel.home
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
        RowLayout {
            Button {
                objectName: "reset"
                text: "重置（框架确认窗口）"
                enabled: root.viewModel.home.count > 0 && !root.viewModel.resetPending
                onClicked: root.viewModel.requestReset()
            }
            Button {
                objectName: "closeWindow"
                text: "关闭"
                onClicked: root.viewModel.closeWindow()
            }
        }
    }
}
