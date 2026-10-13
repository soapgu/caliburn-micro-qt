import QtQuick
import Caliburn.Micro.Qt 1.0

FocusScope {
    id: root

    property alias model: state.model
    readonly property Item item: loading.acceptedItem
    readonly property string errorString: loading.loadError

    function fail(message: string) {
        loading.acceptedItem = null
        loading.loadError = message
        loader.source = ""
        state.reportLoadFailed(message)
    }

    function reloadView() {
        loading.acceptedItem = null
        loader.source = ""
        loading.loadError = ""
        loading.requestedUrl = ""
        if (!state.model)
            return
        const url = ViewLocator.resolve(state.model)
        if (url.toString().length === 0) {
            fail("ViewHost: model has no valid view mapping")
            return
        }
        loading.requestedUrl = url.toString()
        state.reportLoadStarted(url)
        loader.setSource(url, { viewModel: state.model })
    }

    onModelChanged: {
        if (loading.readyToLoad)
            reloadView()
    }
    Component.onCompleted: {
        loading.readyToLoad = true
        reloadView()
    }

    ViewHostState { id: state }

    QtObject {
        id: loading
        property Item acceptedItem: null
        property string loadError: ""
        property string requestedUrl: ""
        property bool readyToLoad: false
    }

    Loader {
        id: loader
        anchors.fill: parent
        focus: true
        asynchronous: false
        onStatusChanged: {
            if (status === Loader.Error)
                root.fail("ViewHost: QML view loading failed " + loading.requestedUrl)
        }
        onLoaded: {
            if (!(item instanceof Item)) {
                root.fail("ViewHost: view root must be an Item")
                return
            }
            if (!state.matchesView(item)) {
                root.fail("ViewHost: injected viewModel does not match the model")
                return
            }
            loading.acceptedItem = item as Item
            state.reportLoadSucceeded(loading.requestedUrl)
        }
    }
}
