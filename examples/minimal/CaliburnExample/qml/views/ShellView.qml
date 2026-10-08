import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Caliburn.Micro.Qt 1.0
import CaliburnExample 1.0

ApplicationWindow {
    id: root
    required property ShellViewModel viewModel

    width: 480
    height: 500
    minimumWidth: 360
    minimumHeight: 460
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
                const detail = shell ? shell.detail : null
                const state = (screen) => !screen ? "无页面"
                    : (screen.isInitialized ? "已初始化" : "未初始化")
                      + " / " + (screen.isActive ? "已激活" : "未激活")
                return "Shell：" + state(shell) + "\nHome：" + state(home) + "\nDetail：" + state(detail)
            }
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
        }

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            Button {
                objectName: "showDetail"
                text: "查看详情"
                enabled: root.viewModel !== null && root.viewModel.canShowDetail
                onClicked: { if (root.viewModel) root.viewModel.showDetail() }
            }
            Button {
                objectName: "goHome"
                text: "返回首页"
                enabled: root.viewModel !== null && root.viewModel.canGoHome
                onClicked: { if (root.viewModel) root.viewModel.goHome() }
            }
        }

        ViewHost {
            objectName: "homeHost"
            model: root.viewModel ? root.viewModel.activeItem : null
            focus: true
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
    }
}
