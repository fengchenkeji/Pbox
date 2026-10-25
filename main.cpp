// main.cpp - Pbox 入口
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QThread>

#include "pbox_paths.h"
#include "container_manager.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName("Pbox");
    app.setOrganizationName("Pbox");

    // 桌面调试用 Material 样式；Android 上由系统主题决定
    QQuickStyle::setStyle("Material");

    // 确保私有目录结构存在
    PboxPaths::ensureDirs();

    QQmlApplicationEngine engine;

    // 后台准备 proot 二进制（从 assets 复制或网络下载），不阻塞 UI
    QThread* prootPrep = QThread::create([]() {
        ContainerManager::instance().ensureProot();
    });
    prootPrep->start();

    const QUrl url(u"qrc:/Pbox/Main.qml"_qs);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);
    engine.load(url);

    int rc = app.exec();
    if (prootPrep->isRunning()) {
        prootPrep->quit();
        prootPrep->wait(2000);
    }
    return rc;
}
