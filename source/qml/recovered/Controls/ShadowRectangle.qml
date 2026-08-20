import QtQuick 2.15
import QtGraphicalEffects 1.15

Item{

    Rectangle {
        id: butterfly_bt
        anchors.fill: butterfly_item
        anchors.margins: 2
        color:"white"
        radius: 8
    }

    Glow{
        anchors.fill: butterfly_bt
        radius: 15
        samples: 34
        color: "#80000000"
        source: butterfly_bt
    }

    Item {
        id: butterfly_item
        anchors.centerIn: parent
        width: 100
        height: 100

        Rectangle {
            id: butterfly
            anchors.fill: parent
            anchors.margins: 1
            color:"white"
            radius: 8
        }



        layer.enabled: true
        layer.effect: InnerShadow {
            color: "#80000000"
            samples: 42
            radius: 15
            spread: 0.4
        }
    }

    /*DropShadow {
        anchors.fill: butterfly
        radius: 12
        samples: 24
        color: "#80000000"
        source: butterfly
    }*/
}
