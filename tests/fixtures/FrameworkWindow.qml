import QtQuick
import QtQuick.Controls
import Caliburn.Micro.Qt 1.0
ApplicationWindow {
    required property ScreenViewModel viewModel
    width: 480
    height: 360
    Column { objectName: "businessLayout"; Item { width: 20; height: 30 } }
}
