// main.cpp - Qt Android App入口
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QDebug>
#include <QtAndroid>

#include "pbox_paths.h"
#include "container_manager.h"
#include "pty_process.h"

// 暴露给QML的终端单例
class TerminalBridge : public QObject {
    Q_OBJECT
public:
    static TerminalBridge& instance() {
        static TerminalBridge inst;
        return inst;
    }

    Q_INVOKABLE void startContainer(const QString& tag) {
        // 创建pty
        int master = posix_openpt(O_RDWR | O_NOCTTY);
        if (master < 0) {
            emit outputReceived("无法创建pty\n");
            return;
        }
        grantpt(master);
        unlockpt(master);

        m_masterFd = master;

        // 启动读取
        m_pty.setOutputCallback([this](const std::string& data) {
            emit outputReceived(QString::fromStdString(data));
        });

        // 启动容器
        ContainerManager::instance().startContainer(tag, master);
    }

    Q_INVOKABLE void sendInput(const QString& text) {
        if (m_masterFd >= 0) {
            ::write(m_masterFd, text.toUtf8().constData(), text.toUtf8().size());
        }
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
    TerminalBridge() : m_pty() {}
    int m_masterFd = -1;
    PtyProcess m_pty;
};

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("Pbox");
    QGuiApplication::setOrganizationName("Pbox");

    // 初始化路径
    PboxPaths::initialize();
    PboxPaths::ensureDirs();

    qInfo() << "Pbox Android App启动";
    qInfo() << "proot版本:" << ContainerManager::instance().prootVersion();

    QQmlApplicationEngine engine;

    // 注册QML上下文
    TerminalBridge::instance();
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
