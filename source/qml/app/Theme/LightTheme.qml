import QtQuick 2.15

QtObject {
    //对应中性 b1-b7(黑色系)
    property color colorB1: "#18181B"
    property color colorB2: "#27272A"
    property color colorB3: "#383838"
    property color colorB4: "#3D3D3D"
    property color colorB5: "#454545"
    property color colorB6: "#4F4F4F"
    property color colorB7: "#5D5D5D"

    //对应中性 W1-W7(白色系)
    property color colorW1: "#FFFFFF"
    property color colorW2: "#F7F7F8"
    property color colorW3: "#E4E4E7"
    property color colorW4: "#6B7280"
    property color colorW5: "#5F6672"
    property color colorW6: "#52525B"
    property color colorW7: "#52525B"

    //主题色 (theme 0)
    property color themeZeroColor: "#5B5BD6"
    //对应 辅助色1-3 (sub1-3)
    property color auxiliaryColor: "#B863FF"
    property color auxiliaryColor2: "#14F1FF"
    property color auxiliaryColor3: "#80FF61"

    //衍生主题色
    property color themeOneColor1: "#5B5BD6"
    property color themeOneColor2: "#7373E6"
    property color themeOneColor3: "#A5A5F3"
    property color themeOneColor4: "#CACAFA"
    property color themeOneColor5: "#E3E3FD"
    property color themeOneColor6: "#F0F0FF"

    property color textColor: "#FFFFFF"
    // Accessibility tokens: each text color is at least 4.5:1 on colorW1.
    property color bodyTextColor: "#18181B"
    property color mutedTextColor: "#52525B"
    property color placeholderTextColor: "#6B7280"
    property color focusRingColor: "#3730A3"
    property color backgroundColor: "#F7F7F8"
    property color surfaceColor: "#FFFFFF"
    property color elevatedColor: "#FFFFFF"
    property color borderColor: "#D4D4D8"
    property color accentColor: "#4F46E5"
    property color disabledSurfaceColor: "#E4E4E7"
    property color disabledTextColor: "#71717A"
    property color successColor: "#047857"
    property color warningColor: "#B45309"
    property color dangerColor: "#B91C1C"
    property color errorSurfaceColor: "#FEF2F2"
    property color shadowColor: "#18000000"
    property color overlayColor: "#52090B10"
    property color selectionColor: "#CACAFA"
    property int spacingXs: 4
    property int spacingSm: 8
    property int spacingMd: 12
    property int spacingLg: 16
    property int radiusSm: 8
    property int radiusMd: 12
    property int radiusLg: 18
    property int fontSmall: 11
    property int fontBody: 13
    property int fontTitle: 18
    property int touchTarget: 40

    property string macro_rBtn_unchecked: "qrc:/Image/Icon/rBtn_unchecked_day.svg"

}



