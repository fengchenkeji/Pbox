// main.cpp - Pbox 入口（修复版）
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QTimer>
#include <QtConcurrent/QtConcurrent>

#include "pbox_paths.h"
#include "container_manager.h"
#include "terminal_bridge.h"

// Qt 6.12 qt_add_qml_module 生成的默认资源前缀
static const QUrl MAIN_QML_URL(QStringLiteral("qrc:/qt/qml/Pbox/Main.qml"));

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
        if (!obj && objUrl == MAIN_QML_URL) {
            QCoreApplication::exit(-1);           // QML 加载失败，保留原兜底
            return;
        }
        if (obj && objUrl == MAIN_QML_URL) {
            // QML 成功创建后再准备 proot，避免启动期阻塞
            QtConcurrent::run([]() {
                ContainerManager::instance().ensureProot();
            });
        }
    }, Qt::QueuedConnection);

    engine.load(MAIN_QML_URL);

    return app.exec();
}
