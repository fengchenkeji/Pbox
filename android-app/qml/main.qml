// main.qml - Pbox Android主界面
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 2.15
import Pbox 1.0

ApplicationWindow {
    id: rootWindow
    width: 480
    height: 800
    visible: true
    title: "Pbox"

    // 颜色主题
    readonly property color bgColor: "#1e1e2e"
    readonly property color cardColor: "#313244"
    readonly property color accentColor: "#89b4fa"
    readonly property color textColor: "#cdd6f4"
    readonly property color successColor: "#a6e3a1"
    readonly property color errorColor: "#f38ba8"

    // 页面栈
    property var pageStack: []

    // ====== 主界面：容器列表 ======
    Component {
        id: mainPage
        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            // 顶部栏
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 56
                color: accentColor
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    Label {
                        text: "Pbox 容器管理"
                        font.pixelSize: 20
                        font.bold: true
                        color: "#1e1e2e"
                    }
                    Item { Layout.fillWidth: true }
                    Button {
                        text: "+ 安装"
                        onClicked: pageStack.push(installPage)
                    }
                }
            }

            // 已安装容器列表
            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: containerManager.listInstalled()
                delegate: ItemDelegate {
                    width: ListView.view.width
                    height: 72
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16
                        ColumnLayout {
                            Label {
                                text: model.display.tag
                                font.pixelSize: 16
                                font.bold: true
                                color: textColor
                            }
                            Label {
                                text: model.display.os + " · " + model.display.release
                                font.pixelSize: 12
                                color: "#7f849c"
                            }
                        }
                        Item { Layout.fillWidth: true }
                        Button {
                            text: "启动"
                            onClicked: {
                                terminalBridge.startContainer(model.display.tag)
                                pageStack.push(terminalPage)
                            }
                        }
                        Button {
                            text: "删除"
                            onClicked: containerManager.removeContainer(model.display.tag)
                        }
                    }
                }
            }
        }
    }

    // ====== 安装页面 ======
    Component {
        id: installPage
        ColumnLayout {
            anchors.fill: parent
            spacing: 12

            // 顶部栏
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 56
                color: cardColor
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 16
                    Button {
                        text: "← 返回"
                        onClicked: pageStack.pop()
                    }
                    Label {
                        text: "安装新容器"
                        font.pixelSize: 18
                        font.bold: true
                        color: textColor
                    }
                }
            }

            // OS选择
            Label {
                text: "选择系统:"
                color: textColor
                font.pixelSize: 14
            }
            ComboBox {
                id: osCombo
                Layout.fillWidth: true
                model: containerManager.listAvailableOS()
            }

            // 版本选择
            Label {
                text: "选择版本:"
                color: textColor
                font.pixelSize: 14
            }
            ComboBox {
                id: releaseCombo
                Layout.fillWidth: true
                model: containerManager.listReleases(osCombo.currentText)
            }

            // 进度条
            ProgressBar {
                id: progressBar
                Layout.fillWidth: true
                visible: false
            }
            Label {
                id: progressLabel
                Layout.fillWidth: true
                color: accentColor
                wrapMode: Label.Wrap
                visible: false
            }

            Item { Layout.fillHeight: true }

            // 安装按钮
            Button {
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                text: "开始安装"
                highlighted: true
                onClicked: {
                    progressBar.visible = true
                    progressLabel.visible = true
                    containerManager.installContainer(osCombo.currentText, releaseCombo.currentText)
                }
            }
        }
    }

    // ====== 终端页面 ======
    Component {
        id: terminalPage
        ColumnLayout {
            anchors.fill: parent

            // 顶部栏
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                color: cardColor
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    Button {
                        text: "←"
                        onClicked: pageStack.pop()
                    }
                    Label {
                        text: "终端"
                        color: textColor
                        font.bold: true
                    }
                }
            }

            // 终端显示区
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "#000000"
                TextArea {
                    id: terminalDisplay
                    anchors.fill: parent
                    readOnly: true
                    color: "#00ff00"
                    font.pixelSize: 12
                    font.family: "monospace"
                    wrapMode: TextArea.Wrap
                    background: Rectangle { color: "#000000" }
                }
            }

            // 输入区
            TextField {
                id: terminalInput
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                placeholderText: "输入命令..."
                color: textColor
                onAccepted: {
                    terminalBridge.sendInput(text + "\n")
                    terminalDisplay.append(text + "\n")
                    text = ""
                }
            }
        }
    }

    // 页面栈管理（简化版）
    Loader {
        anchors.fill: parent
        sourceComponent: pageStack.length > 0 ? pageStack[pageStack.length-1] : mainPage
    }

    // 连接信号
    Connections {
        target: containerManager
        function onInstallFinished(success, message) {
            progressBar.visible = false
            progressLabel.text = message
            if (success) {
                pageStack.pop()
            }
        }
        function onDownloadProgress(percent, message) {
            progressBar.value = percent / 100.0
            progressLabel.text = message
        }
    }

    Connections {
        target: terminalBridge
        function onOutputReceived(data) {
            terminalDisplay.append(data)
        }
    }

    Component.onCompleted: {
        pageStack.push(mainPage)
    }
}
