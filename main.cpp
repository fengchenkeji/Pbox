// main.cpp - Pbox 入口（修复版）
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QTimer>
#include <QtConcurrent/QtConcurrent>

#include "pbox_paths.h"
#include "container_manager.h"
#include "terminal_bridge.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName("Pbox");
    app.setOrganizationName("Pbox");

    QQuickStyle::setStyle("Material");

    PboxPaths::ensureDirs();

    // 【修复1】主线程首次构造两个单例，锁定线程亲和性到主线程
    ContainerManager::instance();
    TerminalBridge::instance();

    QQmlApplicationEngine engine;

    // 【修复2】QML 加载成功后，再异步准备 proot（后台线程，不阻塞 UI）
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [&](QObject *obj, const QUrl &objUrl) {
        if (!obj && objUrl == QUrl(QStringLiteral("qrc:/Pbox/Main.qml"))) {
            QCoreApplication::exit(-1);           // QML 加载失败，保留原兜底
            return;
        }
        if (obj && objUrl == QUrl(QStringLiteral("qrc:/Pbox/Main.qml"))) {
            // QML 成功创建后再准备 proot，避免启动期阻塞
            QtConcurrent::run([]() {
                ContainerManager::instance().ensureProot();
            });
        }
    }, Qt::QueuedConnection);

    engine.load(QUrl(QStringLiteral("qrc:/Pbox/Main.qml")));

    return app.exec();
}
