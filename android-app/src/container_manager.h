// container_manager.h - 容器管理核心
#ifndef CONTAINER_MANAGER_H
#define CONTAINER_MANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QFile>
#include <QElapsedTimer>

struct InstalledContainer {
    Q_GADGET
    Q_PROPERTY(QString tag MEMBER tag)
    Q_PROPERTY(QString os MEMBER os)
    Q_PROPERTY(QString release MEMBER release)
    Q_PROPERTY(QString rootfsPath MEMBER rootfsPath)
    Q_PROPERTY(qint64 sizeBytes MEMBER sizeBytes)
public:
    QString tag;
    QString os;
    QString release;
    QString rootfsPath;
    qint64 sizeBytes = 0;
};
Q_DECLARE_METATYPE(InstalledContainer)

class ContainerManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString arch READ arch CONSTANT)
    Q_PROPERTY(QString prootVersion READ prootVersion CONSTANT)

public:
    static ContainerManager& instance();

    Q_INVOKABLE QStringList listAvailableOS();
    Q_INVOKABLE QStringList listReleases(const QString& os);
    Q_INVOKABLE QVariantList listInstalled();
    Q_INVOKABLE void installContainer(const QString& os, const QString& release);
    Q_INVOKABLE bool startContainer(const QString& tag, qint64 ptyMasterFd);
    Q_INVOKABLE bool removeContainer(const QString& tag);
    Q_INVOKABLE QString prootVersion();
    Q_INVOKABLE QString arch();
    Q_INVOKABLE QString formatSize(qint64 bytes);
    Q_INVOKABLE void cancelDownload();
    Q_INVOKABLE QString logPath() const;

signals:
    void downloadProgress(double percent, const QString& message);
    void installFinished(bool success, const QString& message);
    void logMessage(const QString& msg);
    void containerStarted(const QString& tag);

private slots:
    void onDownloadProgress(qint64 received, qint64 total);
    void onDownloadFinished();
    void onDownloadError(QNetworkReply::NetworkError code);

private:
    explicit ContainerManager(QObject* parent = nullptr);
    QString detectNativeArch();
    QStringList getMirrorUrls(const QString& os, const QString& release, const QString& arch);
    void tryNextMirror();
    bool extractRootfs(const QString& tarPath, const QString& rootfsDir);
    void fixRootfs(const QString& rootfsDir);
    qint64 dirSize(const QString& path);

    QNetworkAccessManager* m_nam;
    QNetworkReply* m_reply = nullptr;
    QFile* m_file = nullptr;
    QElapsedTimer m_timer;

    QStringList m_mirrorUrls;
    int m_mirrorIndex = 0;
    QString m_savePath;
    QString m_rootfsDir;
    QString m_os, m_release, m_arch;
    bool m_cancelled = false;
};

#endif // CONTAINER_MANAGER_H
