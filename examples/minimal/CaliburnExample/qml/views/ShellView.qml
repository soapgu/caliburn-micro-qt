import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Caliburn.Micro.Qt 1.0
import CaliburnExample 1.0

ApplicationWindow {
    id: root
    required property ShellViewModel viewModel

    width: 480
    height: 420
    minimumWidth: 360
    minimumHeight: 380
    visible: true
    title: "Caliburn.Micro.Qt 示例"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        Label {
            objectName: "lifecycleLabel"
            text: {
                const shell = root.viewModel
                const home = shell ? shell.home : null
                const state = (screen) => !screen ? "无页面"
                    : (screen.isInitialized ? "已初始化" : "未初始化")
                      + " / " + (screen.isActive ? "已激活" : "未激活")
                return "Shell：" + state(shell) + "\nHome：" + state(home)
            }
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
        }

        ViewHost {
            objectName: "homeHost"
            model: root.viewModel ? root.viewModel.home : null
            focus: true
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
    }
}
