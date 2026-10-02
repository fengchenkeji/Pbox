// main.cpp - Pbox Android App 入口
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QDebug>

#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <thread>
#include <atomic>
#include <cstring>

#include "pbox_paths.h"
#include "container_manager.h"

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

        ContainerManager::instance().startContainer(tag, master);

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
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("Pbox");
    QGuiApplication::setOrganizationName("Pbox");
    QGuiApplication::setApplicationVersion("1.0.0");

    PboxPaths::initialize();
    PboxPaths::ensureDirs();

    qRegisterMetaType<InstalledContainer>();
    qInfo() << "Pbox v1.0.0 启动, 架构:" << ContainerManager::instance().arch();

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
            if (!obj && url == objUrl)
                QCoreApplication::exit(-1);
        }, Qt::QueuedConnection);
    engine.load(url);

    return app.exec();
}

#include "main.moc"
