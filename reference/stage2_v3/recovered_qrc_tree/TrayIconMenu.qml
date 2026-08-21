import QtQuick 2.15
import QtQuick.Controls 2.15
import "qrc:/Controls"
import "qrc:/GraphicalEffects"

Item {
    width: 196
   // height: list_model.count * 32 + (list_model.count - 1) * 6 + 12
    height:  list_view.contentHeight + 12

    property int deviceMount:device_list_HL.deviceModel.rowCount()

    //加载主题
    Loader {
        id: theme
        source: "qrc:/Theme/"+manager.currentTheme
    }

    //加载字体
    Loader {
        id: fontManager
        source: "qrc:/FontManager.qml"
    }
    //source: "qrc:/Image/menushadow.png"
    //sourceSize: Qt.size(width, height)
    Rectangle{
        id: background_rect
        anchors.fill: parent
        color: theme.item.color7
        border.width: 1
        border.color: "#11000000"
        radius: 8

        property int buttonItemHeight: 32
        property int rectItemHeight: 100

        ListView{
            id:list_view
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.rightMargin: 6
            anchors.leftMargin: 6
            anchors.bottomMargin: 6
            anchors.topMargin: 6
            contentWidth: list_view.width  //196-12
            // contentHeight: background_rect.itemHeight
            interactive: false
            spacing: 6

            model: list_model

            delegate: Item {
                width: list_view.width
                height: group === "0"
                        ? background_rect.buttonItemHeight
                        : background_rect.rectItemHeight

                Button {
                    id: btn
                    visible: group === "0"
                    width: list_view.width
                    height: background_rect.buttonItemHeight
                    anchors.centerIn: parent

                    background: Rectangle {
                        anchors.fill: parent
                        color: btn.pressed
                               ? theme.item.themeOneColor
                               : (btn.hovered
                                  ? theme.item.themeOneColor8
                                  : background_rect.color)
                        radius: 8

                        Image {
                            id: updated_icon
                            visible: name === qsTr("设置") && setting.updated
                            fillMode: Image.PreserveAspectFit
                            anchors.right: parent.right
                            anchors.rightMargin: 4
                            anchors.top: parent.top
                            anchors.topMargin: 4
                            width: 14
                            height: 14
                            source: "qrc:/Image/Settings/updateTip.png"
                            z: 2
                        }

                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: 8
                            anchors.verticalCenter: parent.verticalCenter
                            text: name
                            font.family: fontManager.item.oppoSansM.name
                            font.pixelSize: 14
                            color: theme.item.color1
                        }
                    }

                    onClicked: {
                      //  list_view.currentIndex = index
                        setting.checkUpdate(false)
                        manager.itemClicked(buttonIndex)
                    }
                }

                Rectangle {
                   visible: group === "1"
                   width: list_view.width
                   height: background_rect.rectItemHeight
                   color: theme.item.backgroundColor
                   radius: 8

                   Column {
                       anchors.top: parent.top
                       anchors.topMargin: 8
                       anchors.left:parent.left
                       anchors.leftMargin: 8
                       spacing: 10

                       Row {
                           spacing: 6
                           Image {
                               source: type_image
                               width: 24
                               height: 24
                           }
                           Text {
                               text: name
                               font.family: fontManager.item.oppoSansM.name
                               font.pixelSize: 14
                               anchors.verticalCenter: parent.verticalCenter
                               color: theme.item.color1
                           }
                       }

                       Row {
                           spacing: 6
                           Image {
                               source: connect_type
                               width: 24
                               height: 24
                           }
                           Text {
                               text: battery
                               visible: connect_type === theme.item.wireless_image
                               font.family: fontManager.item.oppoSansM.name
                               font.pixelSize: 14
                               anchors.verticalCenter: parent.verticalCenter
                               color: theme.item.color1
                           }
                           Image {
                              width: 27
                              height: 12
                              anchors.verticalCenter: parent.verticalCenter
                              source: {
                                  if(connect_type === theme.item.wired_image)
                                  {
                                    return theme.item.chargeBattery_image
                                  }

                                  var batteryValue = parseInt(battery.replace('%', ''), 10);
                                  if (isNaN(batteryValue)) {
                                      // 如果没有数值，直接显示100%电池图标
                                      battery = ""
                                      "qrc:/Image/Icon/100battery_image.svg"
                                  } else {
                                      batteryValue === 100
                                          ? "qrc:/Image/Icon/100battery_image.svg"
                                          : (batteryValue >= 80
                                              ? "qrc:/Image/Icon/80battery_image.svg"
                                              : (batteryValue >= 50
                                                  ? "qrc:/Image/Icon/50battery_image.svg"
                                                  : (batteryValue >= 30
                                                      ? "qrc:/Image/Icon/30battery_image.svg"
                                                      : "qrc:/Image/Icon/10battery_image.svg")));
                                  }
                              }
                          }
                          //  Image {
                          //     width: 27
                          //     height: 12
                          //     anchors.verticalCenter: parent.verticalCenter
                          //     source: connect_type === theme.item.wired_image
                          //                 ? theme.item.chargeBattery_image
                          //                 : (parseInt(battery.replace('%', ''), 10) === 100
                          //                     ? "qrc:/Image/Icon/100battery_image.svg"
                          //                     : (parseInt(battery.replace('%', ''), 10) >= 80
                          //                         ? "qrc:/Image/Icon/80battery_image.svg"
                          //                         : (parseInt(battery.replace('%', ''), 10) >= 50
                          //                             ? "qrc:/Image/Icon/50battery_image.svg"
                          //                             : (parseInt(battery.replace('%', ''), 10) >= 30
                          //                                 ? "qrc:/Image/Icon/30battery_image.svg"
                          //                                 : "qrc:/Image/Icon/10battery_image.svg"))))
                          // }
                       }

                       Text {
                           text: device_list_HL.reCurConfigure()
                           font.family: fontManager.item.oppoSansM.name
                           font.pixelSize: 14
                           color: theme.item.color2
                       }
                   }
               }
            }
        }

        ListModel{
            id: list_model
            ListElement{
                group: "0"
                buttonIndex:0
                type_image: ""
                name: qsTr("设置")
                battery: ""
                connect_type: ""
            }
            ListElement{
                group: "0"
                buttonIndex:1
                type_image: ""
                name: qsTr("退出")
                battery: ""
                connect_type: ""
            }
        }
    }

    // 动态添加设备项
    Component.onCompleted: {
        let insertIndex = 1;
        for (var i = 0; i < device_list_HL.deviceModel.rowCount(); i++) {
            list_model.insert(insertIndex, {
                  group: "1",
                  buttonIndex: -1,
                  type_image: device_list_HL.deviceModel.get(i).deviceType === 0 ? theme.item.keyboard_image : theme.item.mouse_image,
                  name: device_list_HL.deviceModel.get(i).deviceName,
                  battery: device_list_HL.deviceModel.get(i).batteryValue + "%",
                  connect_type: device_list_HL.deviceModel.get(i).connectType === "wireless" ? theme.item.wireless_image : theme.item.wired_image
            });
        }
    }

    Connections {
        target: device_list_HL
        function onDeviceListChanged() {
            list_model.clear();
            let insertIndex = 1;
            for (var i = 0; i < device_list_HL.deviceModel.rowCount(); i++) {
                list_model.insert(insertIndex, {
                      group: "1",
                      buttonIndex: -1,
                      type_image: device_list_HL.deviceModel.get(i).deviceType === 0 ? theme.item.keyboard_image : theme.item.mouse_image,
                      name: device_list_HL.deviceModel.get(i).deviceName,
                      battery: device_list_HL.deviceModel.get(i).batteryValue + "%",
                      connect_type: device_list_HL.deviceModel.get(i).connectType === "wireless" ? theme.item.wireless_image : theme.item.wired_image
                });
            }
        }
    }
}
