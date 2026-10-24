import QtQuick
import Caliburn.Micro.Qt 1.0
Window {
    required property ScreenViewModel viewModel
    property string selectedContext: "default"
    width: 320
    height: 240
    visible: false
}
