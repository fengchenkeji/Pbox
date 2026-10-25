// terminal_bridge.h - PTY 终端桥接（QML 单例）
#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqml.h>

#include "pty_process.h"
#include <vector>
#include <string>

class QJSEngine;
class QQmlEngine;

// 负责把 proot 容器 shell（或降级系统 shell）跑在伪终端里，
// 并把子进程输出通过信号发给 QML。
class TerminalBridge : public QObject
{
    Q_OBJECT
    QML_SINGLETON
    QML_ELEMENT
public:
    static TerminalBridge* create(QQmlEngine* engine, QJSEngine* js);
    static TerminalBridge& instance();

    explicit TerminalBridge(QObject* parent = nullptr);

    // 启动一个容器的终端会话（使用 proot 隔离；不可用时降级系统 shell）
    Q_INVOKABLE bool startContainer(const QString& tag);
    // 向当前会话写入一行输入（含换行）
    Q_INVOKABLE void sendInput(const QString& line);
    // 设置终端行列
    Q_INVOKABLE void setWindowSize(int cols, int rows);
    // 停止当前会话
    Q_INVOKABLE void stop();
    // 当前会话是否在运行
    Q_INVOKABLE bool isRunning() const;

signals:
    void outputReceived(const QString& data);
    void sessionStarted(bool success, const QString& message);
    void sessionEnded(const QString& reason);

private:
    void onChildOutput(const std::string& data);
    bool startProcess(const std::vector<std::string>& argv,
                      const QString& workDir, const QString& sessionLabel);

    PtyProcess m_pty;
    bool m_sessionActive = false;
};
