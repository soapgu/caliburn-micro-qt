import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CaliburnExample 1.0

ApplicationWindow {
    id: root
    required property ShellViewModel viewModel

    width: 480
    height: 360
    minimumWidth: 360
    minimumHeight: 320
    visible: true
    title: "Caliburn.Micro.Qt 示例"

    Item {
        id: page
        objectName: "inputScope"
        anchors.fill: parent
        focus: true
        KeyNavigation.tab: incrementButton
        KeyNavigation.backtab: focusInput

        Keys.priority: Keys.AfterItem
        Keys.onPressed: (event) => {
            if (event.key === Qt.Key_2 && event.modifiers === Qt.NoModifier
                    && !event.isAutoRepeat && root.viewModel.canAddTwo) {
                root.viewModel.add(2)
                event.accepted = true
            }
        }

        MouseArea {
            anchors.fill: parent
            onClicked: page.forceActiveFocus()
        }

        ColumnLayout {
            anchors.centerIn: parent
            width: Math.min(parent.width - 32, 400)
            spacing: 20

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
                    id: incrementButton
                    objectName: "increment"
                    text: root.viewModel.incrementText
                    enabled: root.viewModel.canIncrement
                    onClicked: root.viewModel.increment()
                    KeyNavigation.priority: KeyNavigation.BeforeItem
                    KeyNavigation.tab: addTwoButton
                    KeyNavigation.backtab: focusInput
                }

                Button {
                    id: addTwoButton
                    objectName: "addTwo"
                    text: "加 2"
                    enabled: root.viewModel.canAddTwo
                    onClicked: root.viewModel.add(2)
                    KeyNavigation.priority: KeyNavigation.BeforeItem
                    KeyNavigation.tab: resetButton
                }

                Button {
                    id: resetButton
                    objectName: "reset"
                    text: "重置"
                    enabled: root.viewModel.canReset
                    onClicked: root.viewModel.reset()
                    KeyNavigation.priority: KeyNavigation.BeforeItem
                    KeyNavigation.tab: focusInput
                }
            }

            Label {
                text: "按数字键 2 加 2，长按不会连续增加。"
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
            }

            TextField {
                id: focusInput
                objectName: "focusInput"
                placeholderText: "在这里按 2，只输入文字"
                Layout.fillWidth: true
                KeyNavigation.priority: KeyNavigation.BeforeItem
                KeyNavigation.tab: incrementButton
            }

            Label {
                text: "Tab 切换焦点，点击空白处返回页面。"
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }
}
