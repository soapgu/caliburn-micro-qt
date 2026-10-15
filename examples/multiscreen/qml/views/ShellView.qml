pragma ComponentBehavior: Bound
import QtQuick
import CaliburnMultiScreen 1.0

Window {
    id: root
    required property ShellViewModel viewModel
    width: 1440
    height: 810
    // 四屏主窗的最小尺寸；预览按小屏可用区缩小。
    minimumWidth: root.viewModel.modeLabel === "四屏模式" ? 720 : 240
    minimumHeight: root.viewModel.modeLabel === "四屏模式" ? 480 : 180
    title: "CM 多屏示例 - 主操作窗口（" + root.viewModel.modeLabel + "）"
    color: "#991B1B"
    Rectangle {
        anchors.fill: parent
        color: "#991B1B"
        Text {
            anchors.centerIn: parent
            text: "第1块屏幕"
            color: "white"
            font.family: "Noto Sans CJK SC"
            font.pixelSize: Math.min(120, parent.width / 8)
            font.bold: true
            renderType: Text.QtRendering
        }
        Rectangle {
            objectName: "exitButton"
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: Math.min(32, parent.width / 30)
            width: Math.min(240, parent.width / 3)
            height: Math.min(80, parent.height / 5)
            radius: 12
            color: exitMouse.containsMouse ? "#CBD5E1" : "#F1F5F9"
            Text {
                anchors.centerIn: parent
                text: "退出测试"
                color: "#111827"
                font.family: "Noto Sans CJK SC"
                font.pixelSize: Math.min(32, parent.width / 7)
                font.bold: true
            }
            MouseArea {
                id: exitMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.viewModel.exitApplication()
            }
        }
    }
}
