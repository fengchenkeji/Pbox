// terminal_bridge.cpp - PTY 终端桥接实现
#include "terminal_bridge.h"
#include "container_manager.h"
#include "pbox_paths.h"

#include <QDebug>
#include <QDir>
#include <QJSEngine>
#include <QQmlEngine>

#include <unistd.h>
#include <sys/stat.h>
#include <cstdlib>

TerminalBridge* TerminalBridge::create(QQmlEngine*, QJSEngine*)
{
    return &TerminalBridge::instance();
}

TerminalBridge& TerminalBridge::instance()
{
    static TerminalBridge inst;
    return inst;
}

TerminalBridge::TerminalBridge(QObject* parent)
    : QObject(parent)
{
    m_pty.setOutputCallback([this](const std::string& data) {
        onChildOutput(data);
    });
}

void TerminalBridge::onChildOutput(const std::string& data)
{
    emit outputReceived(QString::fromUtf8(data.c_str(), int(data.size())));
}

bool TerminalBridge::startContainer(const QString& tag)
{
    // 已有会话先停掉
    if (m_sessionActive)
        m_pty.stop();

    QString rootfs = PboxPaths::rootfsDir(tag);
    bool rootfsExists = QDir(rootfs).exists();

    // 尝试 proot 隔离启动
    QStringList cmd = ContainerManager::instance().prootCommand(tag);
    if (!cmd.isEmpty()) {
        std::vector<std::string> argv;
        for (const QString& a : cmd) argv.push_back(a.toStdString());
        QString label = "proot(" + tag + ")";
        bool ok = startProcess(argv, rootfs, label);
        if (ok) {
            emit outputReceived(QString("\x1b[32mPbox v1.1 · proot 隔离会话\x1b[0m\n"));
            return true;
        }
    }

    // 降级：系统 shell，工作目录切到 rootfs
    std::vector<std::string> argv = {"/system/bin/sh", "sh"};
    QString banner;
    if (rootfsExists) {
        banner = QString(
            "\x1b[33m[降级模式] proot 不可用，已进入系统 shell（当前目录=rootfs，可 ls/cd 浏览容器文件）\x1b[0m\n");
    } else {
        banner = QString(
            "\x1b[33m[降级模式] 容器 %1 不存在，这是 Android 系统 shell\x1b[0m\n").arg(tag);
    }
    bool ok = startProcess(argv, rootfsExists ? rootfs : QString(), tag);
    if (ok) {
        emit outputReceived(banner);
        return true;
    }
    emit sessionStarted(false, "无法启动终端进程");
    return false;
}

bool TerminalBridge::startProcess(const std::vector<std::string>& argv,
                                  const QString& workDir,
                                  const QString& sessionLabel)
{
    std::vector<std::string> env;
    env.push_back("TERM=xterm-256color");
    env.push_back("HOME=" + PboxPaths::appFilesDir().toStdString());
    env.push_back("PATH=/system/bin:/system/xbin:/sbin:/bin:/usr/bin:/usr/local/bin");
    env.push_back("PS1=pbox:\\w\\$ ");
    env.push_back("LANG=C.UTF-8");
    // proot 需要的加载器环境变量
    env.push_back("PROOT_LOADER=" + std::string(getenv("PROOT_LOADER") ? getenv("PROOT_LOADER") : ""));
    env.push_back("PROOT_LOADER_32=" + std::string(getenv("PROOT_LOADER_32") ? getenv("PROOT_LOADER_32") : ""));
    env.push_back("PROOT_TMP_DIR=" + PboxPaths::cacheDir().toStdString());

    if (!m_pty.start(argv, env)) {
        emit sessionStarted(false, "启动失败: " + sessionLabel);
        return false;
    }
    m_sessionActive = true;
    emit sessionStarted(true, sessionLabel);
    return true;
}

void TerminalBridge::sendInput(const QString& line)
{
    if (!m_sessionActive) return;
    m_pty.write((line + "\n").toStdString());
}

void TerminalBridge::setWindowSize(int cols, int rows)
{
    if (!m_sessionActive) return;
    m_pty.setWindowSize(cols, rows);
}

void TerminalBridge::stop()
{
    if (!m_sessionActive) return;
    m_pty.stop();
    m_sessionActive = false;
    emit sessionEnded("会话已结束");
}

bool TerminalBridge::isRunning() const
{
    return m_sessionActive;
}
