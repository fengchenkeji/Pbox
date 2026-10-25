// container_manager.h - 容器管理核心（QML 单例）
#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QElapsedTimer>
#include <QFile>
#include <QStringList>
#include <QVariantList>
#include <QtQml/qqml.h>

class QJSEngine;
class QQmlEngine;
class QNetworkReply;

// 单例，注册为 QML 类型 Pbox.ContainerManager
class ContainerManager : public QObject
{
    Q_OBJECT
    QML_SINGLETON
    QML_ELEMENT
public:
    static ContainerManager* create(QQmlEngine* engine, QJSEngine* js);
    static ContainerManager& instance();

    explicit ContainerManager(QObject* parent = nullptr);

    // ===== 架构识别 =====
    Q_INVOKABLE QString arch();
    static QString detectNativeArch();

    // ===== 发行版信息 =====
    Q_INVOKABLE QStringList listAvailableOS();
    Q_INVOKABLE QStringList listReleases(const QString& os);

    // ===== 已安装容器 =====
    Q_INVOKABLE QVariantList listInstalled();
    static qint64 dirSizeRecursive(const QString& path);

    // ===== 安装 =====
    Q_INVOKABLE void installContainer(const QString& os, const QString& release);
    Q_INVOKABLE void cancelDownload();

    // ===== 删除 =====
    Q_INVOKABLE bool removeContainer(const QString& tag);

    // ===== 格式化 =====
    Q_INVOKABLE QString formatSize(qint64 bytes);

    // ===== proot 二进制 =====
    // 返回 proot 绝对路径；不可用时返回空串（上层降级系统 shell）
    Q_INVOKABLE QString ensureProot();
    Q_INVOKABLE QString prootVersion();

    // 生成启动某个容器的 proot 命令（argv 首元素为 proot 路径）
    // 返回空列表表示 proot 不可用
    QStringList prootCommand(const QString& tag);

signals:
    void downloadProgress(qreal percent, const QString& message);
    void logMessage(const QString& msg);
    void installFinished(bool success, const QString& message);

private slots:
    void onDownloadProgress(qint64 received, qint64 total);
    void onDownloadFinished();
    void onDownloadError(QNetworkReply::NetworkError code);

private:
    void tryNextMirror();
    void ensureStorageAvailable();
    bool extractRootfs(const QString& tarPath, const QString& rootfsDir);
    void fixRootfs(const QString& rootfsDir);
    bool installProotFromAssets();
    bool installProotFromNetwork();
    static QStringList getMirrorUrls(const QString& os, const QString& release, const QString& arch);

    QNetworkAccessManager* m_nam = nullptr;
    QNetworkReply* m_reply = nullptr;
    QFile* m_file = nullptr;
    QElapsedTimer m_timer;

    QString m_os;
    QString m_release;
    QString m_arch;
    QString m_rootfsDir;
    QString m_savePath;
    QStringList m_mirrorUrls;
    int m_mirrorIndex = 0;
    bool m_cancelled = false;
    bool m_prootReady = false;
};
