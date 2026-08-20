import QtQuick 2.15

QtObject{
    // OPPOSans-Bold
    property var oppoSansB:FontLoader {
        id: oppoSansBLoader
        source: "qrc:/Font/oppo/OPPOSans-B.ttf"
    }

    property var oppoSansH:FontLoader {
        id: oppoSansHLoader
        source: "qrc:/Font/oppo/OPPOSans-H.ttf"
    }

    property var oppoSansL:FontLoader {
        id: oppoSansLLoader
        source: "qrc:/Font/oppo/OPPOSans-L.ttf"
    }

    // OPPOSans-Medium
    property var oppoSansM:FontLoader {
        id: oppoSansMLoader
        source: "qrc:/Font/oppo/OPPOSans-M.ttf"
    }

    property var oppoSansR:FontLoader {
        id: oppoSansRLoader
        source: "qrc:/Font/oppo/OPPOSans-R.ttf"
    }

    property var barlowBoldItalic:FontLoader {
        id: barlowBoldItalicLoader
        source: "qrc:/Font/barlow/Barlow-BoldItalic.otf"
    }

    property var barlowBlackItalic:FontLoader {
        id: barlowBlackItalicLoader
        source: "qrc:/Font/barlow/Barlow-BlackItalic.otf"
    }

    // 取消注释下述字体后barlowBoldItalic无法正常显示
    property var barlowMedium:FontLoader {
        id: barlowMediumLoader
        source: "qrc:/Font/barlow/Barlow-Medium.otf"
    }
}
