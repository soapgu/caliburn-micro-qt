import QtQuick
import Caliburn.Micro.Qt 1.0
Window {
    required property ScreenViewModel viewModel
    property bool initializedAtCreation: false
    property bool activeAtCreation: false
    Component.onCompleted: {
        initializedAtCreation = viewModel.isInitialized
        activeAtCreation = viewModel.isActive
    }
    visible: false
    width: 320
    height: 240
}
