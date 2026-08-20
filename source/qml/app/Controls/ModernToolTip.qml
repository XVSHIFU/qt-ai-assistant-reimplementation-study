import QtQuick 2.15
import QtQuick.Controls 2.15

// Shared compact tooltip used by all modernized controls. A plain non-modal
// Popup is used instead of Qt 5 ToolTip: the latter can paint an opaque overlay
// on transparent Qt::Tool windows and can be clipped near compact-window edges.
Popup {
    id: control
    parent: Overlay.overlay
    property Item anchorItem: null
    property bool displayAbove: false
    property bool forceVisible: false
    property real scaleFactor: window.scale
    property string text: ""
    property int delay: 0
    property int timeout: 2600

    modal: false
    dim: false
    focus: false
    closePolicy: Popup.NoAutoClose

    // mapToItem() inside a declarative binding is not re-evaluated when an
    // ancestor of anchorItem moves (for example a ListView delegate scrolling,
    // or the conversation toggle changing between compact/expanded layouts).
    // Cache a freshly mapped scene position whenever the popup is shown and
    // while it remains visible, so the bubble stays attached to its control.
    property point anchorPosition: Qt.point(0, 0)

    function refreshAnchorPosition() {
        if (!anchorItem || !parent)
            return
        anchorPosition = anchorItem.mapToItem(parent, 0, 0)
    }

    onVisibleChanged: {
        if (visible) {
            refreshAnchorPosition()
            Qt.callLater(refreshAnchorPosition)
        }
    }
    onOpened: refreshAnchorPosition()

    Timer {
        interval: 16
        repeat: true
        running: control.visible
        onTriggered: control.refreshAnchorPosition()
    }

    readonly property real preferredX: anchorPosition.x
                                       + ((anchorItem ? anchorItem.width : 0) - implicitWidth) / 2
    readonly property real belowY: anchorPosition.y
                                   + (anchorItem ? anchorItem.height : 0) + 6 * scaleFactor
    readonly property real aboveY: anchorPosition.y - implicitHeight - 6 * scaleFactor
    readonly property bool useAbove: displayAbove
                                     ? aboveY >= 6 * scaleFactor
                                     : belowY + implicitHeight > parent.height - 6 * scaleFactor

    x: Math.round(Math.max(6 * scaleFactor,
                          Math.min(parent.width - implicitWidth - 6 * scaleFactor,
                                   preferredX)))
    y: Math.round(useAbove ? Math.max(6 * scaleFactor, aboveY)
                           : Math.min(parent.height - implicitHeight - 6 * scaleFactor,
                                      belowY))
    padding: 0
    margins: 8 * scaleFactor
    implicitWidth: Math.min(parent ? parent.width - 12 * scaleFactor : 320 * scaleFactor,
                            label.implicitWidth + 16 * scaleFactor)
    implicitHeight: 26 * scaleFactor

    contentItem: Text {
        id: label
        text: control.text
        color: "#FFFFFF"
        font.family: fontManager.item.uiFontFamily
        font.pixelSize: 12 * control.scaleFactor
        font.weight: Font.Medium
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        color: "#18181B"
        radius: 6 * control.scaleFactor
        border.width: 1
        border.color: "#2F2F33"
    }
}
