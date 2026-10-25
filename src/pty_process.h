// pty_process.h - 伪终端进程封装
#pragma once
#include <QString>
#include <functional>
#include <thread>
#include <atomic>
#include <vector>
#include <string>

// 封装一个在伪终端(PTY)中运行的前台进程，负责 fork/setsid、
// master/slave 打开、子进程 I/O 重定向，以及读线程与写接口。
class PtyProcess
{
public:
    using OutputCallback = std::function<void(const std::string& data)>;

    PtyProcess();
    ~PtyProcess();

    PtyProcess(const PtyProcess&) = delete;
    PtyProcess& operator=(const PtyProcess&) = delete;

    // 启动 argv 指定的进程（首元素为可执行文件路径）
    bool start(const std::vector<std::string>& argv,
               const std::vector<std::string>& env = {});

    // 向 PTY 写入数据（模拟键盘输入）
    bool write(const std::string& data);

    // 设置终端窗口尺寸（列、行）
    bool setWindowSize(int cols, int rows);

    void setOutputCallback(OutputCallback cb);

    // 停止并回收子进程
    void stop();

    bool isRunning() const;

    // 阻塞等待子进程退出，返回退出码
    int wait();

    // master 端 fd（供上层直接读取/写入，如使用容器文件浏览时）
    int masterFd() const { return m_masterFd; }

private:
    void readLoop();

    int         m_masterFd = -1;
    pid_t       m_childPid = -1;
    std::atomic<bool> m_running{false};
    std::thread m_readThread;
    OutputCallback m_outputCb;
};
