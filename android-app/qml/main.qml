// main.qml - Pbox 主界面（参考 tiny_container 设计风格）
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 2.15
import Pbox 1.0

ApplicationWindow {
    id: win
    width: 480
    height: 860
    visible: true
    title: qsTr("Pbox")

    // ===== 主题色 =====
    readonly property color bg:        "#1e1e2e"
    readonly property color surface:  "#313244"
    readonly property color surface2: "#45475a"
    readonly property color accent:   "#89b4fa"
    readonly property color green:    "#a6e3a1"
    readonly property color red:      "#f38ba8"
    readonly property color yellow:   "#f9e2af"
    readonly property color text:     "#cdd6f4"
    readonly property color subtext:  "#7f849c"

    // 页面栈: "home" / "install" / "terminal"
    property string currentPage: "home"
    property string activeContainer: ""

    // ===== 顶部栏 =====
    Rectangle {
        id: topBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 56
        color: surface

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            spacing: 12

            // 返回按钮（非首页显示）
            Button {
                text: "<"
                visible: win.currentPage !== "home"
                onClicked: {
                    if (win.currentPage === "install") win.currentPage = "home"
                    else if (win.currentPage === "terminal") win.currentPage = "home"
                }
            }

            Label {
                text: win.currentPage === "home" ? "Pbox"
                    : win.currentPage === "install" ? "安装新容器"
                    : "终端 - " + win.activeContainer
                font.pixelSize: 20
                font.bold: true
                color: text
                Layout.fillWidth: true
            }

            // 关于按钮
            Button {
                text: "ⓘ"
                visible: win.currentPage === "home"
                onClicked: aboutDialog.open()
            }

            // 安装按钮（首页）
            Button {
                text: "+ 安装"
                visible: win.currentPage === "home"
                highlighted: true
                onClicked: {
                    installPage.reset()
                    win.currentPage = "install"
                }
            }
        }
    }

    // ===== 首页：容器列表 =====
    Rectangle {
        id: homePage
        anchors.top: topBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        visible: win.currentPage === "home"
        color: bg

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 12

            // 状态信息条
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 44
                radius: 8
                color: surface
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    Label {
                        text: "架构: " + containerManager.arch
                        color: subtext
                        font.pixelSize: 12
                    }
                    Item { Layout.fillWidth: true }
                    Label {
                        text: containerManager.ptraceAvailable ? "proot: " + containerManager.prootVersion
                                                              : "⚠ ptrace 被禁止"
                        color: containerManager.ptraceAvailable ? subtext : red
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                }
            }

            Label {
                text: "已安装容器"
                font.pixelSize: 16
                font.bold: true
                color: text
            }

            // 容器列表
            ListView {
                id: containerList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 10
                model: containerManager.listInstalled()
                verticalScrollBar.visible: true

                delegate: Rectangle {
                    width: ListView.view.width
                    height: 76
                    radius: 10
                    color: surface
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 14
                        anchors.rightMargin: 14
                        anchors.topMargin: 10
                        spacing: 4

                        RowLayout {
                            Label {
                                text: model.tag
                                font.pixelSize: 16
                                font.bold: true
                                color: text
                                Layout.fillWidth: true
                            }
                            Label {
                                text: containerManager.formatSize(model.sizeBytes)
                                color: subtext
                                font.pixelSize: 11
                            }
                        }
                        RowLayout {
                            Label {
                                text: model.os + " · " + model.release
                                color: subtext
                                font.pixelSize: 12
                                Layout.fillWidth: true
                            }
                            Button {
                                text: "启动"
                                highlighted: true
                                onClicked: {
                                    win.activeContainer = model.tag
                                    terminalPage.clear()
                                    terminalBridge.startContainer(model.tag)
                                    win.currentPage = "terminal"
                                }
                            }
                            Button {
                                text: "删除"
                                onClicked: {
                                    confirmDialog.message = "确定删除容器 " + model.tag + " ?"
                                    confirmDialog.onAccept = function() {
                                        containerManager.removeContainer(model.tag)
                                        containerList.model = containerManager.listInstalled()
                                    }
                                    confirmDialog.open()
                                }
                            }
                        }
                    }
                }

                // 空状态
                Text {
                    visible: count === 0
                    anchors.centerIn: parent
                    text: "还没有容器\n点击右上角 + 安装"
                    color: subtext
                    horizontalAlignment: Text.AlignHCenter
                    font.pixelSize: 14
                }
            }
        }
    }

    // ===== 安装页 =====
    Rectangle {
        id: installPage
        anchors.top: topBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        visible: win.currentPage === "install"
        color: bg

        function reset() {
            progressBar.visible = false
            progressLabel.visible = false
            cancelBtn.visible = false
            progressBar.value = 0
            installBtn.enabled = true
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 14

            Label {
                text: "选择 Linux 发行版"
                font.pixelSize: 16
                font.bold: true
                color: text
            }

            Label { text: "发行版:"; color: subtext; font.pixelSize: 13 }
            ComboBox {
                id: osCombo
                Layout.fillWidth: true
                model: containerManager.listAvailableOS()
                onCurrentTextChanged: {
                    releaseCombo.model = containerManager.listReleases(currentText)
                }
            }

            Label { text: "版本:"; color: subtext; font.pixelSize: 13 }
            ComboBox {
                id: releaseCombo
                Layout.fillWidth: true
                model: containerManager.listReleases(osCombo.currentText)
            }

            // 进度
            ProgressBar {
                id: progressBar
                Layout.fillWidth: true
                visible: false
                from: 0
                to: 100
            }
            Label {
                id: progressLabel
                Layout.fillWidth: true
                color: accent
                font.pixelSize: 12
                wrapMode: Label.Wrap
                visible: false
            }

            // 日志
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: logView.text.length > 0
                Rectangle {
                    width: parent.width
                    color: "transparent"
                    Label {
                        id: logView
                        text: ""
                        color: subtext
                        font.pixelSize: 11
                        wrapMode: Label.Wrap
                        width: parent.width
                    }
                }
            }

            Item { Layout.fillHeight: true }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                Button {
                    id: cancelBtn
                    visible: false
                    text: "取消"
                    Layout.fillWidth: true
                    onClicked: containerManager.cancelDownload()
                }
                Button {
                    id: installBtn
                    text: "开始安装"
                    highlighted: true
                    Layout.fillWidth: true
                    Layout.preferredHeight: 48
                    onClicked: {
                        progressBar.visible = true
                        progressLabel.visible = true
                        cancelBtn.visible = true
                        installBtn.enabled = false
                        logView.text = ""
                        containerManager.installContainer(osCombo.currentText, releaseCombo.currentText)
                    }
                }
            }
        }
    }

    // ===== 终端页 =====
    Rectangle {
        id: terminalPage
        anchors.top: topBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        visible: win.currentPage === "terminal"
        color: "#000000"

        function clear() {
            terminalDisplay.text = ""
        }

        ColumnLayout {
            anchors.fill: parent

            // 终端输出
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                TextArea {
                    id: terminalDisplay
                    readOnly: true
                    color: "#00ff00"
                    font.pixelSize: 13
                    font.family: "monospace"
                    wrapMode: TextArea.WrapAnywhere
                    background: Rectangle { color: "#000000" }
                    selectionColor: "#004400"
                }
            }

            // 输入栏
            TextField {
                id: terminalInput
                Layout.fillWidth: true
                Layout.preferredHeight: 44
                placeholderText: "输入命令..."
                color: "#00ff00"
                placeholderTextColor: "#006600"
                font.family: "monospace"
                background: Rectangle { color: "#0a0a0a"; border.color: "#004400" }
                onAccepted: {
                    terminalBridge.sendInput(text + "\n")
                    text = ""
                }
            }
        }
    }

    // ===== 关于对话框 =====
    Dialog {
        id: aboutDialog
        title: "关于 Pbox"
        standardButtons: Dialog.Ok
        Modal { dim: true }
        contentItem: ColumnLayout {
            spacing: 10
            Label { text: "Pbox v1.0.0"; font.bold: true; font.pixelSize: 16; color: text }
            Label { text: "无 Root 的 proot 容器管理器"; color: subtext; wrapMode: Label.Wrap }
            Label { text: "架构: " + containerManager.arch; color: subtext; font.pixelSize: 12 }
            Label { text: "proot: " + containerManager.prootVersion; color: subtext; font.pixelSize: 12 }
            Label { text: "参考: tiny_container / Pbox"; color: subtext; font.pixelSize: 11 }
        }
    }

    // ===== 确认对话框 =====
    Dialog {
        id: confirmDialog
        title: "确认"
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        property var onAccept: function() {}
        onAccepted: onAccept()
        contentItem: Label {
            text: "确定?"
            color: text
            wrapMode: Label.Wrap
        }
    }

    // ===== 信号连接 =====
    Connections {
        target: containerManager
        function onDownloadProgress(percent, message) {
            progressBar.value = percent
            progressLabel.text = message
        }
        function onInstallFinished(success, message) {
            progressLabel.text = message
            cancelBtn.visible = false
            installBtn.enabled = true
            if (success) {
                containerList.model = containerManager.listInstalled()
                win.currentPage = "home"
            }
        }
        function onLogMessage(msg) {
            logView.text += msg + "\n"
        }
    }

    Connections {
        target: terminalBridge
        function onOutputReceived(data) {
            terminalDisplay.append(data)
            terminalDisplay.cursorPosition = terminalDisplay.text.length
        }
    }
}
