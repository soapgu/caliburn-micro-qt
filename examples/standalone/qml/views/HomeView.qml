import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CaliburnConsumer 1.0

Item {
    id: root
    objectName: "consumerHome"
    required property HomeViewModel viewModel
    ColumnLayout {
        anchors.centerIn: parent
        Label {
            objectName: "countLabel"
            text: "已点击 " + root.viewModel.count + " 次"
            font.pixelSize: 24
        }
        Button {
            objectName: "increment"
            text: "增加"
            onClicked: root.viewModel.increment()
        }
    }
}
