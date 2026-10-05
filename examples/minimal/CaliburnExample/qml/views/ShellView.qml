import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CaliburnExample 1.0

ApplicationWindow {
    id: root
    required property ShellViewModel viewModel

    width: 480
    height: 320
    minimumWidth: 360
    minimumHeight: 240
    visible: true
    title: "Caliburn.Micro.Qt 示例"

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 24

        Label {
            objectName: "messageLabel"
            text: root.viewModel.message
            font.pixelSize: 28
            Layout.alignment: Qt.AlignHCenter
        }

        RowLayout {
            spacing: 16
            Layout.alignment: Qt.AlignHCenter

            Button {
                objectName: "increment"
                text: root.viewModel.incrementText
                enabled: root.viewModel.canIncrement
                onClicked: root.viewModel.increment()
            }

            Button {
                objectName: "reset"
                text: "重置"
                enabled: root.viewModel.canReset
                onClicked: root.viewModel.reset()
            }
        }
    }
}
