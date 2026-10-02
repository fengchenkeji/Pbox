// pty_process.cpp - 伪终端进程管理实现
#include "pty_process.h"
#include <QDebug>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <cstdlib>
#include <cstring>
#include <signal.h>

PtyProcess::PtyProcess() {}

PtyProcess::~PtyProcess()
{
    stop();
}

bool PtyProcess::start(const std::vector<std::string>& argv,
                       const std::vector<std::string>& env)
{
    if (m_running) {
        qWarning() << "进程已在运行";
        return false;
    }

    if (argv.empty()) {
        qWarning() << "argv为空";
        return false;
    }

    // 打开pty master端
    int master = posix_openpt(O_RDWR | O_NOCTTY);
    if (master < 0) {
        qWarning() << "posix_openpt失败:" << strerror(errno);
        return false;
    }

    if (grantpt(master) != 0 || unlockpt(master) != 0) {
        qWarning() << "grantpt/unlockpt失败:" << strerror(errno);
        close(master);
        return false;
    }

    char* slaveName = ptsname(master);
    if (!slaveName) {
        qWarning() << "ptsname失败";
        close(master);
        return false;
    }

    std::string slavePath(slaveName);

    pid_t pid = fork();
    if (pid < 0) {
        qWarning() << "fork失败:" << strerror(errno);
        close(master);
        return false;
    }

    if (pid == 0) {
        // ===== 子进程 =====
        close(master);

        // 打开pty slave端
        int slave = open(slavePath.c_str(), O_RDWR);
        if (slave < 0) {
            _exit(127);
        }

        // 创建新会话，成为控制终端
        setsid();
        ioctl(slave, TIOCSCTTY, 0);

        // 将stdin/stdout/stderr重定向到pty slave
        dup2(slave, STDIN_FILENO);
        dup2(slave, STDOUT_FILENO);
        dup2(slave, STDERR_FILENO);
        if (slave > STDERR_FILENO) close(slave);

        // 设置默认终端大小
        struct winsize ws = {24, 80, 0, 0};
        ioctl(STDIN_FILENO, TIOCSWINSZ, &ws);

        // 构建argv
        std::vector<char*> args;
        for (const auto& a : argv) {
            args.push_back(const_cast<char*>(a.c_str()));
        }
        args.push_back(nullptr);

        // 构建环境变量
        std::vector<char*> envp;
        for (const auto& e : env) {
            envp.push_back(const_cast<char*>(e.c_str()));
        }
        envp.push_back(nullptr);

        // 执行
        execve(args[0], args.data(), envp.data());

        // exec失败
        _exit(127);
    }

    // ===== 父进程 =====
    m_masterFd = master;
    m_childPid = pid;
    m_running = true;

    // 启动读取线程
    m_readThread = std::thread(&PtyProcess::readLoop, this);

    qInfo() << "子进程启动成功, pid=" << pid
            << "cmd:" << QString::fromStdString(argv[0]);

    return true;
}

void PtyProcess::readLoop()
{
    char buf[4096];
    while (m_running) {
        ssize_t n = read(m_masterFd, buf, sizeof(buf));
        if (n > 0) {
            std::string data(buf, n);
            if (m_outputCb) {
                m_outputCb(data);
            }
        } else if (n == 0) {
            qInfo() << "pty对端关闭";
            break;
        } else {
            if (errno == EINTR) continue;
            if (errno == EIO) {
                // 子进程退出
                qInfo() << "pty EIO, 子进程已退出";
                break;
            }
            qWarning() << "read失败:" << strerror(errno);
            break;
        }
    }
    m_running = false;
}

bool PtyProcess::write(const std::string& data)
{
    if (!m_running || m_masterFd < 0) return false;
    ssize_t n = ::write(m_masterFd, data.data(), data.size());
    return n == static_cast<ssize_t>(data.size());
}

bool PtyProcess::setWindowSize(int cols, int rows)
{
    if (m_masterFd < 0) return false;
    struct winsize ws = {
        static_cast<unsigned short>(rows),
        static_cast<unsigned short>(cols),
        0, 0
    };
    return ioctl(m_masterFd, TIOCSWINSZ, &ws) == 0;
}

void PtyProcess::setOutputCallback(std::function<void(const std::string&)> cb)
{
    m_outputCb = cb;
}

void PtyProcess::stop()
{
    if (!m_running && m_childPid <= 0) return;

    m_running = false;

    if (m_childPid > 0) {
        // 先尝试正常终止
        kill(m_childPid, SIGTERM);

        // 等待2秒，不退出则强杀
        for (int i = 0; i < 20; i++) {
            int status;
            pid_t r = waitpid(m_childPid, &status, WNOHANG);
            if (r == m_childPid) {
                m_childPid = -1;
                break;
            }
            usleep(100000);
        }
        if (m_childPid > 0) {
            kill(m_childPid, SIGKILL);
            waitpid(m_childPid, nullptr, 0);
            m_childPid = -1;
        }
    }

    if (m_masterFd >= 0) {
        close(m_masterFd);
        m_masterFd = -1;
    }

    if (m_readThread.joinable()) {
        m_readThread.join();
    }
}

bool PtyProcess::isRunning() const
{
    return m_running;
}

int PtyProcess::wait()
{
    if (m_childPid <= 0) return -1;
    int status = 0;
    waitpid(m_childPid, &status, 0);
    m_childPid = -1;
    m_running = false;
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}
