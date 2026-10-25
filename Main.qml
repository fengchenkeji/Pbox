import QtQuick
import QtQuick.Window

Window {
    width: 800
    height: 480
    visible: true
    title: "Proot容器管理器"
    color: "#fafafa"

    property int currentTab: 0

    // 页面容器
    StackLayout {
        id: pageStack
        anchors.fill: parent
        anchors.bottomMargin: 110

        // ========== 容器页面 ==========
        Column {
            spacing: 16
            padding: 20
            anchors.fill: parent

            Text {
                text: "我的容器"
                font.pixelSize: 22
                font.weight: Font.Medium
                color: "#1c1b1f"
            }

            // M3卡片示例，后续放容器列表
            Rectangle {
                width: parent.width
                height: 90
                radius: 16
                color: "#ffffff"
                border.color: "#e7e0ec"
                border.width: 1

                Row {
                    anchors.fill: parent
                    anchors.leftMargin:16
                    anchors.rightMargin:16
                    anchors.verticalCenter: parent.verticalCenter
                    spacing:16

                    Rectangle {
                        width:40
                        height:40
                        radius:12
                        color:"#eaddff"
                    }
                    Column {
                        spacing:4
                        Text {
                            text: "Ubuntu 25.10"
                            font.pixelSize:16
                            font.weight:Font.Medium
                            color:"#1c1b1f"
                        }
                        Text {
                            text: "proot-distro 容器"
                            font.pixelSize:13
                            color:"#49454f"
                        }
                    }
                }
            }
        }

        // ========== 日志页面 ==========
        Column {
            spacing: 16
            padding: 20
            anchors.fill: parent
            Text {
                text: "运行日志"
                font.pixelSize: 22
                font.weight: Font.Medium
                color: "#1c1b1f"
            }
            Rectangle {
                width: parent.width
                height: parent.height
                radius:12
                color:"#1c1b1f"
                Text {
                    anchors.fill: parent
                    anchors.margins:12
                    text: "# 日志输出区域\n等待容器启动..."
                    font.family: "monospace"
                    font.pixelSize:13
                    color:"#e6e1e5"
                }
            }
        }

        // ========== 设置页面 ==========
        Column {
            spacing: 16
            padding: 20
            anchors.fill: parent
            Text {
                text: "应用设置"
                font.pixelSize: 22
                font.weight: Font.Medium
                color: "#1c1b1f"
            }
            Rectangle {
                width: parent.width
                height: 64
                radius:16
                color:"#ffffff"
                border.color: "#e7e0ec"
                border.width:1
                Row {
                    anchors.fill: parent
                    anchors.leftMargin:16
                    anchors.rightMargin:16
                    anchors.verticalCenter: parent.verticalCenter
                    Text {
                        text: "深色模式"
                        font.pixelSize:15
                        color:"#1c1b1f"
                    }
                }
            }
        }
    }

    // 悬浮底部导航栏（和你截图样式对齐）
    Rectangle {
        id: navBar
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        width: 420
        height: 76
        anchors.bottomMargin:20
        radius: 28
        color: "#ffffff"
        // 模拟柔和阴影
        Rectangle {
            anchors.fill: parent
            anchors.leftMargin: 2
            anchors.topMargin: 2
            radius: 28
            color: "#00000018"
            z: -1
        }

        Row {
            anchors.fill: parent
            spacing:0
            Repeater {
                model: [
                    { text:"容器" },
                    { text:"日志" },
                    { text:"设置" }
                ]
                Rectangle {
                    width: navBar.width / 3
                    height: navBar.height
                    color: "transparent"

                    Column {
                        anchors.centerIn: parent
                        spacing:4
                        // M3选中圆形底色
                        Rectangle {
                            width: 56
                            height:32
                            radius:16
                            visible: index === currentTab
                            color:"#eaddff"
                        }
                        Text {
                            text: modelData.text
                            anchors.horizontalCenter: parent.horizontalCenter
                            font.pixelSize:13
                            font.weight: index === currentTab ? Font.Medium : Font.Normal
                            color: index === currentTab ? "#6750a4" : "#49454f"
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            currentTab = index
                            pageStack.currentIndex = index
                            console.log("切换页面：", modelData.text)
                        }
                    }
                }
            }
        }
    }
}
