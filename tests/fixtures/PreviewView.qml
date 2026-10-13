import QtQuick
import LocatorTest 1.0
Text {
    required property PreviewViewModel viewModel
    text: viewModel.title
}
