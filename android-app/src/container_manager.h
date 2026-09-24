// container_manager.h - 容器管理核心（Qt接口封装）
#ifndef CONTAINER_MANAGER_H
#define CONTAINER_MANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <functional>

// 镜像信息
struct ImageInfo {
    QString os;       // ubuntu, debian, alpine...
    QString release;  // jammy, bookworm...
    QString arch;     // arm64, armhf...
    QString path;     // simplestreams路径
};

// 已安装容器
struct InstalledContainer {
    QString tag;      // ubuntu_jammy
    QString os;
    QString release;
    QString rootfsPath;
    qint64 sizeBytes = 0;
};

class ContainerManager : public QObject {
    Q_OBJECT
public:
    static ContainerManager& instance();

    // 列出可用OS
    Q_INVOKABLE QStringList listAvailableOS();

    // 列出指定OS的版本
    Q_INVOKABLE QStringList listReleases(const QString& os);

    // 列出已安装容器
    Q_INVOKABLE QList<InstalledContainer> listInstalled();

    // 安装容器（异步，进度通过信号回调）
    Q_INVOKABLE void installContainer(const QString& os, const QString& release);

    // 启动容器（通过pty）
    Q_INVOKABLE bool startContainer(const QString& tag, qint64 ptyMasterFd);

    // 删除容器
    Q_INVOKABLE bool removeContainer(const QString& tag);

    // 获取proot版本
    Q_INVOKABLE QString prootVersion();

signals:
    // 下载进度 (0.0 - 1.0)
    void downloadProgress(double percent, const QString& message);
    // 安装完成
    void installFinished(bool success, const QString& message);
    // 日志输出
    void logMessage(const QString& msg);

private:
    explicit ContainerManager(QObject* parent = nullptr);
    QString detectNativeArch();
    QString findImagePath(const QString& os, const QString& release, const QString& arch);
    bool downloadRootfs(const QString& url, const QString& savePath);
    bool extractRootfs(const QString& tarPath, const QString& rootfsDir);
    void buildProotArgs(const QString& rootfsPath, QStringList& args);
};

#endif // CONTAINER_MANAGER_H
