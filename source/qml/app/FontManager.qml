import QtQuick 2.15

// Compatibility facade for the recovered QML font contract. The original
// implementation eagerly loaded five large OPPO Sans files and three Barlow
// files on every window, adding substantial startup and resident-memory cost.
// Keep the old property names so recovered screens continue to work, while
// routing them to the native Windows families selected by main.cpp.
QtObject {
    id: root

    // Qt 5's offscreen/software text engine can expose an empty system-font
    // fallback set. Register one recovered family so glyph fallback remains
    // available in smoke/compatibility mode; production text still requests
    // the native family first. This replaces the former eight eager loaders.
    property FontLoader compatibilityFallback: FontLoader {
        source: "qrc:/Font/oppo/OPPOSans-R.ttf"
    }

    readonly property string uiFontFamily: Qt.application.uiFontFamily || "Segoe UI"
    readonly property string codeFontFamily: Qt.application.codeFontFamily || "Cascadia Mono"

    property QtObject oppoSansB: QtObject { readonly property string name: root.uiFontFamily }
    property QtObject oppoSansH: QtObject { readonly property string name: root.uiFontFamily }
    property QtObject oppoSansL: QtObject { readonly property string name: root.uiFontFamily }
    property QtObject oppoSansM: QtObject { readonly property string name: root.uiFontFamily }
    property QtObject oppoSansR: QtObject { readonly property string name: root.uiFontFamily }

    property QtObject barlowBoldItalic: QtObject { readonly property string name: root.uiFontFamily }
    property QtObject barlowBlackItalic: QtObject { readonly property string name: root.uiFontFamily }
    property QtObject barlowMedium: QtObject { readonly property string name: root.uiFontFamily }
}
