import QtQuick
import QtQuick.Controls
import Caliburn.Micro.Qt 1.0

Item {
    id: root
    anchors.fill: parent
    property alias windowManager: state.manager
    property Item fallbackFocusItem: null
    readonly property bool opened: popup.opened
    readonly property Item dialogItem: view.item
    property string activeRequest: ""
    property Item previousFocus: null

    function restoreFocus() {
        if (activeRequest.length !== 0 || !root.Window.window || !root.Window.window.visible)
            return
        if (previousFocus && previousFocus.visible && previousFocus.enabled)
            previousFocus.forceActiveFocus(Qt.PopupFocusReason)
        else if (fallbackFocusItem && fallbackFocusItem.visible && fallbackFocusItem.enabled)
            fallbackFocusItem.forceActiveFocus(Qt.PopupFocusReason)
        previousFocus = null
    }

    function showCurrent() {
        if (!state.model)
            return
        activeRequest = state.requestId
        previousFocus = root.Window.window ? root.Window.window.activeFocusItem : null
        view.model = state.model
        if (view.errorString.length > 0) {
            state.failed(activeRequest, view.errorString)
            return
        }
        if (view.item)
            popup.open()
    }

    function hideCurrent(requestId: string) {
        if (requestId !== activeRequest)
            return
        popup.close()
        view.model = null
        activeRequest = ""
        state.released(requestId)
        Qt.callLater(root.restoreFocus)
    }

    DialogHostState {
        id: state
        available: root.Window.window !== null
        onModelChanged: { if (model) root.showCurrent() }
        onHideRequested: (requestId) => root.hideCurrent(requestId)
    }

    Popup {
        id: popup
        objectName: "dialogPopup"
        parent: Overlay.overlay
        anchors.centerIn: parent
        popupType: Popup.Item
        modal: true
        dim: true
        focus: true
        closePolicy: Popup.NoAutoClose
        enter: null
        exit: null
        padding: 20
        width: Math.min((view.item && view.item.implicitWidth > 0 ? view.item.implicitWidth : 360)
                        + leftPadding + rightPadding, parent ? parent.width - 24 : 400)
        height: Math.min((view.item && view.item.implicitHeight > 0 ? view.item.implicitHeight : 220)
                         + topPadding + bottomPadding, parent ? parent.height - 24 : 260)
        onOpened: { if (view.item) view.item.forceActiveFocus(Qt.PopupFocusReason) }
        contentItem: FocusScope {
            id: modalScope
            focus: true
            function inside(item: Item): bool {
                for (let node = item; node; node = node.parent) {
                    if (node === modalScope)
                        return true
                }
                return false
            }
            function moveFocus(forward: bool) {
                const start = root.Window.window ? root.Window.window.activeFocusItem : null
                if (!start) {
                    modalScope.forceActiveFocus()
                    return
                }
                let next = start.nextItemInFocusChain(forward)
                while (next && next !== start) {
                    if (inside(next) && next.visible && next.enabled && next.activeFocusOnTab) {
                        next.forceActiveFocus(forward ? Qt.TabFocusReason : Qt.BacktabFocusReason)
                        return
                    }
                    next = next.nextItemInFocusChain(forward)
                }
                start.forceActiveFocus()
            }
            Keys.onPressed: (event) => {
                if (event.key === Qt.Key_Escape)
                    state.dismiss(root.activeRequest)
                else if (event.key === Qt.Key_Tab || event.key === Qt.Key_Backtab)
                    moveFocus(event.key === Qt.Key_Tab && !(event.modifiers & Qt.ShiftModifier))
                event.accepted = true
            }
            ViewHost {
                id: view
                anchors.fill: parent
                focus: true
                onErrorStringChanged: {
                    if (errorString.length > 0 && root.activeRequest.length > 0)
                        state.failed(root.activeRequest, errorString)
                }
            }
        }
    }
    Component.onDestruction: {
        popup.close()
        view.model = null
        state.manager = null
    }
}
