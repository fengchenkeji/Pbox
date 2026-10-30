// Main.qml - Pbox 主界面（Material3 风格，悬浮底部导航）
import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts
import Pbox 1.0

Window {
    id: win
    width: 420
    height: 860
    visible: true
    title: qsTr("Pbox")
    // M3 浅色主题色（可切换深色）
    readonly property color primary:    dark ? "#D0BCFF" : "#6750A4"
    readonly property color onPrimary:  dark ? "#381E72" : "#FFFFFF"
    readonly property color surface:    dark ? "#141218" : "#FDF8FD"
    readonly property color surfaceCtr: dark ? "#211F26" : "#F3EDF7"
    readonly property color surfaceHigh:dark ? "#36343B" : "#E6E0E9"
    readonly property color onSurface:  dark ? "#E6E0E9" : "#1C1B1F"
    readonly property color onSurfaceV: dark ? "#CAC4D0" : "#49454F"
    readonly property color secondaryC: dark ? "#381E72" : "#EADDFF"
    property bool dark: false

    property int currentTab: 0
    property string activeContainer: ""
    property string logText: ""
    property string installLog: ""

    color: surface

    // ===================== 页面容器 =====================
    StackLayout {
        id: pageStack
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
            bottom: navBar.top
            bottomMargin: 12
        }
        currentIndex: currentTab

        // ---------- 页面1：容器 ----------
        Column {
            spacing: 16
            padding: 20

            Text {
                text: "我的容器"
                font.pixelSize: 22
                font.weight: Font.Medium
                color: onSurface
            }

            // 状态条
            Rectangle {
                width: parent.width
                height: 44
                radius: 12
                color: surfaceCtr
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 14
                    Label {
                        text: "架构 " + ContainerManager.arch()
                        color: onSurfaceV
                        font.pixelSize: 12
                        Layout.fillWidth: true
                    }
                    Label {
                        text: ContainerManager.prootVersion()
                        color: onSurfaceV
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                }
            }

            // 容器列表
            ListView {
                id: containerList
                width: parent.width
                height: parent.height - 160
                clip: true
                spacing: 12
                model: ContainerManager.listInstalled()
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; visible: true }

                delegate: Rectangle {
                    width: ListView.view.width
                    height: 84
                    radius: 16
                    color: surfaceCtr
                    border.width: 0

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 12
                        spacing: 12
                        Rectangle {
                            width: 40
                            height: 40
                            radius: 12
                            color: secondaryC
                            Text {
                                anchors.centerIn: parent
                                text: model.os.length > 0 ? model.os.charAt(0).toUpperCase() : "?"
                                font.bold: true
                                color: primary
                            }
                        }
                        Column {
                            spacing: 4
                            Layout.fillWidth: true
                            Text {
                                text: model.tag
                                font.pixelSize: 16
                                font.weight: Font.Medium
                                color: onSurface
                            }
                            Text {
                                text: (model.os ? model.os : "") + " · "
                                      + (model.release ? model.release : "")
                                      + " · " + ContainerManager.formatSize(model.sizeBytes)
                                font.pixelSize: 12
                                color: onSurfaceV
                            }
                        }
                        Button {
                            text: "启动"
                            highlighted: true
                            onClicked: {
                                win.activeContainer = model.tag
                                TerminalBridge.startContainer(model.tag)
                                win.currentTab = 2 // 终端页
                            }
                        }
                        Button {
                            text: "删除"
                            onClicked: {
                                ContainerManager.removeContainer(model.tag)
                                containerList.model = ContainerManager.listInstalled()
                            }
                        }
                    }
                }
                // 空状态
                Text {
                    visible: containerList.count === 0
                    anchors.centerIn: parent
                    text: "还没有容器\n点击下方「安装」添加"
                    color: onSurfaceV
                    horizontalAlignment: Text.AlignHCenter
                    font.pixelSize: 14
                }
            }
        }

        // ---------- 页面2：安装 ----------
        Column {
            spacing: 14
            padding: 20

            Text {
                text: "安装新容器"
                font.pixelSize: 22
                font.weight: Font.Medium
                color: onSurface
            }
            Text { text: "发行版"; color: onSurfaceV; font.pixelSize: 13 }
            ComboBox {
                id: osCombo
                width: parent.width
                model: ContainerManager.listAvailableOS()
                onCurrentTextChanged: {
                    releaseCombo.model = ContainerManager.listReleases(currentText)
                }
            }
            Text { text: "版本"; color: onSurfaceV; font.pixelSize: 13 }
            ComboBox {
                id: releaseCombo
                width: parent.width
                model: ContainerManager.listReleases(osCombo.currentText)
            }
            ProgressBar {
                id: progressBar
                width: parent.width
                from: 0
                to: 100
                visible: false
            }
            Text {
                id: progressLabel
                width: parent.width
                color: primary
                font.pixelSize: 12
                wrapMode: Text.Wrap
                visible: false
            }
            // 安装日志
            ScrollView {
                width: parent.width
                height: parent.height - 340
                visible: installLog.length > 0
                contentItem: Flickable {
                    clip: true
                    contentHeight: logTextItem.paintedHeight
                    Text {
                        id: logTextItem
                        text: installLog
                        color: onSurfaceV
                        font.pixelSize: 11
                        wrapMode: Text.Wrap
                        width: win.width - 40
                    }
                }
            }
            Item { Layout.fillHeight: true }
            RowLayout {
                width: parent.width
                spacing: 10
                Button {
                    id: cancelBtn
                    visible: false
                    text: "取消"
                    Layout.fillWidth: true
                    onClicked: ContainerManager.cancelDownload()
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
                        installLog = ""
                        ContainerManager.installContainer(osCombo.currentText, releaseCombo.currentText)
                    }
                }
            }
        }

        // ---------- 页面3：终端 ----------
        Column {
            spacing: 0
            Text {
                text: "终端" + (activeContainer ? " · " + activeContainer : "")
                font.pixelSize: 22
                font.weight: Font.Medium
                color: onSurface
                padding: 20
            }
            Rectangle {
                width: parent.width
                height: parent.height - 120
                color: "#0D1117"
                radius: 12
                Flickable {
                    id: termFlick
                    anchors.fill: parent
                    anchors.margins: 8
                    contentWidth: termOut.paintedWidth
                    contentHeight: termOut.paintedHeight
                    clip: true
                    Text {
                        id: termOut
                        text: ""
                        color: "#00E676"
                        font.family: "monospace"
                        font.pixelSize: 13
                        wrapMode: Text.Wrap
                    }
                }
            }
            TextField {
                id: termInput
                width: parent.width
                height: 48
                placeholderText: "输入命令，回车执行..."
                placeholderTextColor: "#777"
                color: "#00E676"
                font.family: "monospace"
                background: Rectangle { color: "#161B22"; radius: 8 }
                onAccepted: {
                    TerminalBridge.sendInput(text)
                    text = ""
                }
            }
        }

        // ---------- 页面4：日志 ----------
        Column {
            spacing: 12
            padding: 20
            Text {
                text: "运行日志"
                font.pixelSize: 22
                font.weight: Font.Medium
                color: onSurface
            }
            Rectangle {
                width: parent.width
                height: parent.height - 60
                radius: 12
                color: surfaceCtr
                Flickable {
                    anchors.fill: parent
                    anchors.margins: 12
                    contentHeight: logView.paintedHeight
                    clip: true
                    Text {
                        id: logView
                        text: logText
                        color: onSurfaceV
                        font.family: "monospace"
                        font.pixelSize: 12
                        wrapMode: Text.Wrap
                        width: parent.width
                    }
                }
            }
        }

        // ---------- 页面5：设置 ----------
        Column {
            spacing: 14
            padding: 20
            Text {
                text: "应用设置"
                font.pixelSize: 22
                font.weight: Font.Medium
                color: onSurface
            }
            Rectangle {
                width: parent.width
                height: 60
                radius: 16
                color: surfaceCtr
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    Label { text: "深色模式"; color: onSurface; font.pixelSize: 15; Layout.fillWidth: true }
                    Switch {
                        checked: dark
                        onCheckedChanged: {
                            // 通过 JS 改属性（M3 明暗切换）
                            win.dark = checked
                            win.color = win.surface
                        }
                    }
                }
            }
            Rectangle {
                width: parent.width
                height: 60
                radius: 16
                color: surfaceCtr
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    Label { text: "proot 状态"; color: onSurfaceV; font.pixelSize: 13; Layout.fillWidth: true }
                    Label { text: ContainerManager.prootVersion(); color: primary; font.pixelSize: 13 }
                }
            }
            Rectangle {
                width: parent.width
                height: 60
                radius: 16
                color: surfaceCtr
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    Label { text: "关于"; color: onSurfaceV; font.pixelSize: 13; Layout.fillWidth: true }
                    Label { text: "Pbox v1.1 · 无Root proot 容器"; color: onSurfaceV; font.pixelSize: 12 }
                }
            }
        }
    }

    // ===================== M3 悬浮底部导航 =====================
    Rectangle {
        id: navBar
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        width: win.width - 32
        height: 76
        anchors.bottomMargin: 20
        radius: 28
        color: surfaceCtr
        // 柔和阴影（基础组件模拟）
        Rectangle {
            anchors.fill: parent
            anchors.leftMargin: 2
            anchors.topMargin: 2
            radius: 28
            color: "#1F000000"
            z: -1
        }

        Row {
            anchors.fill: parent
            spacing: 0
            Repeater {
                model: [
                    { text: "容器", icon: "◧" },
                    { text: "安装", icon: "＋" },
                    { text: "终端", icon: "⌁" },
                    { text: "日志", icon: "≡" },
                    { text: "设置", icon: "⚙" }
                ]
                delegate: Item {
                    width: navBar.width / 5
                    height: navBar.height
                    // M3 选中圆形底座
                    Rectangle {
                        width: 64
                        height: 34
                        radius: 17
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.top
                        anchors.topMargin: 10
                        visible: index === currentTab
                        color: secondaryC
                    }
                    Column {
                        anchors.centerIn: parent
                        spacing: 2
                        Text {
                            text: modelData.icon
                            anchors.horizontalCenter: parent.horizontalCenter
                            font.pixelSize: 20
                            color: index === currentTab ? primary : onSurfaceV
                        }
                        Text {
                            text: modelData.text
                            anchors.horizontalCenter: parent.horizontalCenter
                            font.pixelSize: 11
                            font.weight: index === currentTab ? Font.Medium : Font.Normal
                            color: index === currentTab ? primary : onSurfaceV
                        }
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            currentTab = index
                            // 切到容器页时刷新列表
                            if (index === 0)
                                containerList.model = ContainerManager.listInstalled()
                        }
                    }
                }
            }
        }
    }

    // ===================== 信号连接 =====================
    Connections {
        target: ContainerManager
        function onDownloadProgress(percent, message) {
            progressBar.value = percent
            progressLabel.text = message
        }
        function onInstallFinished(success, message) {
            progressLabel.text = message
            cancelBtn.visible = false
            installBtn.enabled = true
            progressBar.visible = false
            if (success) {
                containerList.model = ContainerManager.listInstalled()
                currentTab = 0
            }
        }
        function onLogMessage(msg) {
            installLog += msg + "\n"
            logText += msg + "\n"
        }
    }
    Connections {
        target: TerminalBridge
        function onOutputReceived(data) {
            termOut.text += data
            termFlick.contentY = termFlick.contentHeight
        }
        function onSessionStarted(success, message) {
            logText += (success ? "[终端] 启动 " : "[终端] 失败 ") + message + "\n"
        }
        function onSessionEnded(reason) {
            logText += "[终端] " + reason + "\n"
        }
    }
}
