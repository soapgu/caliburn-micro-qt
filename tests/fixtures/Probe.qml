import QtQuick
import Caliburn.Micro.Qt 1.0
FocusScope {
    required property ScreenViewModel viewModel
    implicitWidth: 260
    implicitHeight: 160
    focus: true
    TextInput { objectName: "dialogInput"; focus: true }
}
