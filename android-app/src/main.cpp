// main.cpp - Pbox Android App 入口
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QDebug>
#include <QFile>

#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <thread>
#include <atomic>
#include <cstring>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <unwind.h>
#include <dlfcn.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <sys/types.h>

#include "pbox_paths.h"
#include "container_manager.h"

// ===== 崩溃捕获：写入 files/logs/crash.log =====
// 用 _Unwind_Backtrace（编译器内置），不依赖 bionic 的 execinfo.h（NDK r29 不再暴露 backtrace）
static int g_crashFd = -1;

struct CrashBtState {
    void** current;
    void** end;
};

static _Unwind_Reason_Code crashUnwind(struct _Unwind_Context* ctx, void* arg)
{
    CrashBtState* st = static_cast<CrashBtState*>(arg);
    uintptr_t pc = _Unwind_GetIP(ctx);
    if (pc != 0) {
        if (st->current >= st->end) return _URC_END_OF_STACK;
        *st->current++ = reinterpret_cast<void*>(pc);
    }
    return _URC_NO_REASON;
}

static int captureCrashBacktrace(void** buffer, int max)
{
    CrashBtState st = {buffer, buffer + max};
    _Unwind_Backtrace(crashUnwind, &st);
    return static_cast<int>(st.current - buffer);
}

static void crashHandler(int sig)
{
    char buf[128];
    int n = snprintf(buf, sizeof(buf), "\n===== CRASH signal=%d (%s) =====\n",
                     sig, strsignal(sig));
    if (g_crashFd >= 0) {
        write(g_crashFd, buf, n);
        void* bt[64];
        int cnt = captureCrashBacktrace(bt, 64);
        for (int i = 0; i < cnt; i++) {
            char line[320];
            Dl_info info;
            if (dladdr(bt[i], &info) && info.dli_sname) {
                snprintf(line, sizeof(line), "  #%02d %p %s+0x%lx (%s)\n",
                         i, bt[i], info.dli_sname,
                         (unsigned long)((char*)bt[i] - (char*)info.dli_saddr),
                         info.dli_fname ? info.dli_fname : "?");
            } else if (dladdr(bt[i], &info) && info.dli_fname) {
                snprintf(line, sizeof(line), "  #%02d %p (%s+0x%lx)\n",
                         i, bt[i], info.dli_fname,
                         (unsigned long)((char*)bt[i] - (char*)info.dli_fbase));
            } else {
                snprintf(line, sizeof(line), "  #%02d %p\n", i, bt[i]);
            }
            write(g_crashFd, line, strlen(line));
        }
        fsync(g_crashFd);
        close(g_crashFd);
    }
    _exit(128 + sig);
}

static void initCrashLog()
{
    QString logPath = QString::fromStdString(PboxPaths::logDir()) + "/crash.log";
    g_crashFd = open(logPath.toUtf8().constData(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    signal(SIGSEGV, crashHandler);
    signal(SIGABRT, crashHandler);
    signal(SIGBUS, crashHandler);
    signal(SIGILL, crashHandler);
    signal(SIGFPE, crashHandler);
}

// ===== Qt 日志：写入 files/logs/qt.log =====
static FILE* g_qtLog = nullptr;

static const char* qTypeStr(QtMsgType t)
{
    switch (t) {
    case QtDebugMsg: return "DEBUG";
    case QtInfoMsg: return "INFO";
    case QtWarningMsg: return "WARN";
    case QtCriticalMsg: return "ERROR";
    case QtFatalMsg: return "FATAL";
    }
    return "?";
}

static void qtLogHandler(QtMsgType type, const QMessageLogContext& ctx, const QString& msg)
{
    Q_UNUSED(ctx);
    if (g_qtLog) {
        fprintf(g_qtLog, "[%s] %s\n", qTypeStr(type), msg.toUtf8().constData());
        fflush(g_qtLog);
    }
    if (type == QtFatalMsg) abort();
}

// ===== ptrace 可用性预检（proot 依赖 ptrace）=====
static bool checkPtraceAvailable()
{
    pid_t pid = fork();
    if (pid < 0) return false;
    if (pid == 0) {
        // 子进程：TRACEME 成功则退出码0，否则1
        if (ptrace(PTRACE_TRACEME, 0, 0, 0) == 0)
            _exit(0);
        _exit(1);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

// ===== 终端桥接 =====
class TerminalBridge : public QObject {
    Q_OBJECT
public:
    static TerminalBridge& instance() {
        static TerminalBridge inst;
        return inst;
    }

    Q_INVOKABLE void startContainer(const QString& tag) {
        stopReader();

        int master = posix_openpt(O_RDWR | O_NOCTTY);
        if (master < 0) {
            emit outputReceived(QStringLiteral("\x1b[31m无法创建 pty: %1\x1b[0m\n").arg(strerror(errno)));
            return;
        }
        grantpt(master);
        unlockpt(master);
        m_masterFd = master;

        if (!ContainerManager::instance().startContainer(tag, master)) {
            emit outputReceived(QStringLiteral("\x1b[31m容器启动失败，请查看日志\x1b[0m\n"));
            close(m_masterFd);
            m_masterFd = -1;
            return;
        }

        m_running = true;
        m_readThread = std::thread([this]() {
            char buf[8192];
            while (m_running) {
                ssize_t n = read(m_masterFd, buf, sizeof(buf));
                if (n > 0) {
                    emit outputReceived(QString::fromUtf8(buf, n));
                } else if (n == 0 || errno == EIO) {
                    break;
                } else if (errno == EINTR) {
                    continue;
                } else {
                    break;
                }
            }
            m_running = false;
            emit outputReceived(QStringLiteral("\r\n\x1b[33m[进程已退出]\x1b[0m\n"));
        });
    }

    Q_INVOKABLE void sendInput(const QString& text) {
        if (m_masterFd >= 0)
            ::write(m_masterFd, text.toUtf8().constData(), text.toUtf8().size());
    }

    Q_INVOKABLE void resizeTerminal(int cols, int rows) {
        if (m_masterFd >= 0) {
            struct winsize ws = {(unsigned short)rows, (unsigned short)cols, 0, 0};
            ioctl(m_masterFd, TIOCSWINSZ, &ws);
        }
    }

signals:
    void outputReceived(const QString& data);

private:
    TerminalBridge() {}
    ~TerminalBridge() { stopReader(); }
    void stopReader() {
        m_running = false;
        if (m_masterFd >= 0) {
            close(m_masterFd);
            m_masterFd = -1;
        }
        if (m_readThread.joinable()) m_readThread.join();
    }
    int m_masterFd = -1;
    std::atomic<bool> m_running{false};
    std::thread m_readThread;
};

int main(int argc, char* argv[])
{
    // 卓易通等兼容环境 GPU/OpenGL 支持不完整，强制软件渲染避免启动闪退
    qputenv("QT_QUICK_BACKEND", "software");
    qputenv("QSG_RHI_BACKEND", "software");

    // 路径初始化（纯文件操作，先于 QGuiApplication）
    PboxPaths::initialize();
    PboxPaths::ensureDirs();

    // 崩溃捕获 + Qt 日志
    initCrashLog();
    QString logPath = QString::fromStdString(PboxPaths::logDir()) + "/qt.log";
    g_qtLog = fopen(logPath.toUtf8().constData(), "w");
    qInstallMessageHandler(qtLogHandler);

    // ptrace 预检
    bool ptraceOk = checkPtraceAvailable();
    ContainerManager::instance().setPtraceAvailable(ptraceOk);

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("Pbox");
    QGuiApplication::setOrganizationName("Pbox");
    QGuiApplication::setApplicationVersion("1.0.1");

    qRegisterMetaType<InstalledContainer>();
    qInfo() << "Pbox v1.0.1 启动, 架构:" << ContainerManager::instance().arch()
            << ", ptrace:" << (ptraceOk ? "OK" : "DENIED");
    qInfo() << "appFilesDir:" << QString::fromStdString(PboxPaths::appFilesDir());
    qInfo() << "nativeLibDir:" << QString::fromStdString(PboxPaths::nativeLibDir());
    qInfo() << "proot存在:" << QFile::exists(QString::fromStdString(PboxPaths::prootBin()))
            << " loader存在:" << QFile::exists(QString::fromStdString(PboxPaths::prootLoader()));

    QQmlApplicationEngine engine;

    qmlRegisterUncreatableType<TerminalBridge>("Pbox", 1, 0, "TerminalBridge",
        "Use TerminalBridge.instance");
    qmlRegisterUncreatableType<ContainerManager>("Pbox", 1, 0, "ContainerManager",
        "Use ContainerManager.instance");

    engine.rootContext()->setContextProperty("terminalBridge", &TerminalBridge::instance());
    engine.rootContext()->setContextProperty("containerManager", &ContainerManager::instance());

    const QUrl url(QStringLiteral("qrc:/qml/main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
        &app, [url](QObject* obj, const QUrl& objUrl) {
            if (!obj && url == objUrl) {
                qCritical() << "QML 加载失败";
                QCoreApplication::exit(-1);
            }
        }, Qt::QueuedConnection);
    engine.load(url);

    return app.exec();
}

#include "main.moc"
