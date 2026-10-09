import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Caliburn.Micro.Qt 1.0

FocusScope {
    id: root
    required property ConfirmActionViewModel viewModel
    implicitWidth: 360
    implicitHeight: content.implicitHeight
    focus: true
    ColumnLayout {
        id: content
        anchors.fill: parent
        spacing: 20
        Label {
            text: root.viewModel ? root.viewModel.title : ""
            font.bold: true
            font.pixelSize: 20
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        Label {
            objectName: "confirmationMessage"
            text: root.viewModel ? root.viewModel.message : ""
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        RowLayout {
            Layout.alignment: Qt.AlignRight
            Button {
                id: acceptButton
                objectName: "dialogAccept"
                text: root.viewModel ? root.viewModel.confirmText : "确认"
                onClicked: { if (root.viewModel) root.viewModel.accept() }
                Keys.onReturnPressed: { if (root.viewModel) root.viewModel.accept() }
                Keys.onEnterPressed: { if (root.viewModel) root.viewModel.accept() }
                KeyNavigation.priority: KeyNavigation.BeforeItem
                KeyNavigation.tab: cancelButton
                KeyNavigation.backtab: cancelButton
            }
            Button {
                id: cancelButton
                objectName: "dialogCancel"
                text: root.viewModel ? root.viewModel.cancelText : "取消"
                focus: true
                onClicked: { if (root.viewModel) root.viewModel.cancel() }
                Keys.onReturnPressed: { if (root.viewModel) root.viewModel.cancel() }
                Keys.onEnterPressed: { if (root.viewModel) root.viewModel.cancel() }
                KeyNavigation.priority: KeyNavigation.BeforeItem
                KeyNavigation.tab: acceptButton
                KeyNavigation.backtab: acceptButton
            }
        }
    }
}
