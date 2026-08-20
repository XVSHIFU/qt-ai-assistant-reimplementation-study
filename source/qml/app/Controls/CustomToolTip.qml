import QtQuick 2.15
import QtQuick.Controls 2.15

ModernToolTip {
    property alias tipBackground: backgroundItem
    property alias tipTextView: textItem
    property bool displayTop: false
    displayAbove: displayTop

    // Compatibility aliases for recovered ImageButton/PushButton users.
    readonly property Item backgroundItem: background
    readonly property Item textItem: contentItem
}
