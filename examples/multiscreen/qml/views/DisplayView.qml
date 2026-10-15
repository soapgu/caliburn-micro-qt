pragma ComponentBehavior: Bound
import QtQuick
import CaliburnMultiScreen 1.0

Window {
    id: root
    required property DisplayViewModel viewModel
    width: 5760
    height: 1080
    title: "CM 多屏示例 - 扩展显示（" + root.viewModel.modeLabel + "）"
    color: "black"
    readonly property list<color> panelColors: ["#166534", "#1E40AF", "#6B21A8"]
    Row {
        anchors.fill: parent
        Repeater {
            model: 3
            Rectangle {
                id: panel
                required property int index
                width: root.width / 3
                height: root.height
                color: root.panelColors[index]
                Text {
                    anchors.centerIn: parent
                    text: "第" + (panel.index + 2) + "块屏幕"
                    color: "white"
                    font.family: "Noto Sans CJK SC"
                    font.pixelSize: Math.min(120, panel.width / 8)
                    font.bold: true
                    renderType: Text.QtRendering
                }
            }
        }
    }
}
