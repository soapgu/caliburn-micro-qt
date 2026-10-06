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
        console.warn(message)
    }

    function reloadView() {
        loading.acceptedItem = null
        loader.source = ""
        loading.loadError = ""
        loading.requestedUrl = ""
        if (!state.model)
            return
        const url = ViewRegistry.resolve(state.model)
        if (url.toString().length === 0) {
            fail("ViewHost：VM 缺少有效 View 映射")
            return
        }
        loading.requestedUrl = url.toString()
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
                root.fail("ViewHost：QML View 加载失败 " + loading.requestedUrl)
        }
        onLoaded: {
            if (!(item instanceof Item)) {
                root.fail("ViewHost：View 根对象必须为 Item")
                return
            }
            if (!state.matchesView(item)) {
                root.fail("ViewHost：View 的 viewModel 注入不匹配")
                return
            }
            loading.acceptedItem = item as Item
        }
    }
}
