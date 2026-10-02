// pty_process.h - 伪终端进程管理（交互式终端核心）
#ifndef PTY_PROCESS_H
#define PTY_PROCESS_H

#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <thread>

class PtyProcess {
public:
    PtyProcess();
    ~PtyProcess();

    // 启动子进程，通过pty交互
    // argv: 命令及参数列表
    // env: 额外环境变量
    bool start(const std::vector<std::string>& argv,
               const std::vector<std::string>& env = {});

    // 向pty写入数据（用户输入）
    bool write(const std::string& data);

    // 读取pty输出（线程安全，有数据时回调）
    void setOutputCallback(std::function<void(const std::string&)> cb);

    // 调整终端窗口大小
    bool setWindowSize(int cols, int rows);

    // 停止子进程
    void stop();

    // 是否正在运行
    bool isRunning() const;

    // 等待子进程退出
    int wait();

private:
    int m_masterFd = -1;   // pty master端
    pid_t m_childPid = -1; // 子进程PID
    std::atomic<bool> m_running{false};
    std::thread m_readThread;
    std::function<void(const std::string&)> m_outputCb;

    void readLoop();
};

#endif // PTY_PROCESS_H
