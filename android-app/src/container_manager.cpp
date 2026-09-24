// container_manager.cpp - 容器管理核心实现
#include "container_manager.h"
#include "pbox_paths.h"

#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QStorageInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QProcess>
#include <QDir>

#include <unistd.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

ContainerManager& ContainerManager::instance()
{
    static ContainerManager inst;
    return inst;
}

ContainerManager::ContainerManager(QObject* parent)
    : QObject(parent)
{
}

QString ContainerManager::detectNativeArch()
{
    struct utsname u;
    if (uname(&u) != 0) return "arm64";

    QString machine(u.machine);
    if (machine == "aarch64" || machine == "arm64") return "arm64";
    if (machine.startsWith("arm")) return "armhf";
    if (machine == "x86_64") return "amd64";
    if (machine == "riscv64") return "riscv64";
    return machine;
}

QStringList ContainerManager::listAvailableOS()
{
    // 简化版：返回支持的发行版列表
    // 实际应从images_cache.json解析
    return {"ubuntu", "debian", "alpine", "fedora", "archlinux"};
}

QStringList ContainerManager::listReleases(const QString& os)
{
    QMap<QString, QStringList> releases = {
        {"ubuntu", {"jammy", "noble", "focal", "mantic"}},
        {"debian", {"bookworm", "bullseye", "trixie"}},
        {"alpine", {"3.19", "3.20", "edge"}},
        {"fedora", {"39", "40"}},
        {"archlinux", {"latest"}}
    };
    return releases.value(os, {});
}

QList<InstalledContainer> ContainerManager::listInstalled()
{
    QList<InstalledContainer> result;
    QDir rootfsRoot(PboxPaths::rootfsRoot().c_str());

    for (const auto& entry : rootfsRoot.entryInfoList(QDir::AllDirs | QDir::NoDotAndDotDot)) {
        InstalledContainer c;
        c.tag = entry.fileName();
        c.rootfsPath = entry.absoluteFilePath();

        // 解析tag: ubuntu_jammy
        QStringList parts = c.tag.split('_');
        if (parts.size() >= 2) {
            c.os = parts[0];
            c.release = parts.mid(1).join("_");
        }

        // 计算大小
        QDir dir(entry.absoluteFilePath());
        c.sizeBytes = 0;
        // 简化：递归计算太耗时，这里只标记存在

        result.append(c);
    }
    return result;
}

QString ContainerManager::findImagePath(const QString& os, const QString& release, const QString& arch)
{
    // LXC simplestreams路径格式
    // images/ubuntu/jammy/arm64/default/20240101_07:00/root.tar.xz
    // 简化：使用每日构建的latest
    return QString("images/%1/%2/%3/default/")
        .arg(os.toLower())
        .arg(release.toLower())
        .arg(arch);
}

bool ContainerManager::downloadRootfs(const QString& url, const QString& savePath)
{
    emit downloadProgress(0, QString("开始下载: %1").arg(url));

    QNetworkAccessManager nam;
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "Pbox/1.0 (Android)");

    QNetworkReply* reply = nam.get(request);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::downloadProgress, [this](qint64 received, qint64 total) {
        if (total > 0) {
            double pct = (double)received / total * 100.0;
            emit downloadProgress(pct, QString("下载中: %1 / %2 MB")
                .arg(received / 1024 / 1024)
                .arg(total / 1024 / 1024));
        }
    });
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        emit logMessage(QString("下载失败: %1").arg(reply->errorString()));
        reply->deleteLater();
        return false;
    }

    QFile file(savePath);
    if (!file.open(QIODevice::WriteOnly)) {
        emit logMessage(QString("无法写入文件: %1").arg(savePath));
        reply->deleteLater();
        return false;
    }
    file.write(reply->readAll());
    file.close();
    reply->deleteLater();

    emit downloadProgress(100, "下载完成");
    return true;
}

bool ContainerManager::extractRootfs(const QString& tarPath, const QString& rootfsDir)
{
    emit logMessage("开始解压rootfs...");

    QDir().mkpath(rootfsDir);

    // 使用proot tar解压（和原项目逻辑一致）
    QString proot = QString::fromStdString(PboxPaths::prootBin());

    QStringList args;
    args << "--link2symlink"
         << "tar"
         << "-pJxf"
         << tarPath
         << "-C" << rootfsDir;

    // 用QProcess运行（通过shell）
    QProcess proc;
    proc.setProgram(proot);
    proc.setArguments(args);

    QObject::connect(&proc, &QProcess::readyReadStandardOutput, [&]() {
        emit logMessage(QString::fromUtf8(proc.readAllStandardOutput()));
    });
    QObject::connect(&proc, &QProcess::readyReadStandardError, [&]() {
        emit logMessage(QString::fromUtf8(proc.readAllStandardError()));
    });

    proc.start();
    proc.waitForFinished(-1);

    if (proc.exitCode() != 0) {
        emit logMessage(QString("解压失败，退出码: %1").arg(proc.exitCode()));
        return false;
    }

    // 修复resolv.conf
    QString resolv = rootfsDir + "/etc/resolv.conf";
    QFile::remove(resolv);
    QFile rf(resolv);
    if (rf.open(QIODevice::WriteOnly | QIODevice::Text)) {
        rf.write("nameserver 114.114.114.114\n");
        rf.write("nameserver 223.5.5.5\n");
        rf.close();
    }

    emit logMessage("解压完成");
    return true;
}

void ContainerManager::installContainer(const QString& os, const QString& release)
{
    QString arch = detectNativeArch();
    QString tag = QString("%1_%2").arg(os.toLower(), release.toLower());
    QString rootfs = QString::fromStdString(PboxPaths::rootfsDir(tag.toStdString()));

    emit logMessage(QString("安装容器: %1 [%2]").arg(tag, arch));

    // 镜像源列表
    QStringList mirrors = {
        "https://mirrors.tuna.tsinghua.edu.cn/lxc-images/",
        "https://mirror.nyist.edu.cn/lxc-images/",
        "https://images.linuxcontainers.org/"
    };

    QString imageRelPath = findImagePath(os, release, arch);
    QString tarPath = QString::fromStdString(PboxPaths::cacheDir()) + "/" + tag + ".tar.xz";

    bool downloaded = false;
    for (const QString& mirror : mirrors) {
        QString url = mirror + imageRelPath + "root.tar.xz";
        emit logMessage(QString("尝试镜像: %1").arg(mirror));
        if (downloadRootfs(url, tarPath)) {
            downloaded = true;
            break;
        }
    }

    if (!downloaded) {
        emit installFinished(false, "所有镜像源下载失败");
        return;
    }

    if (!extractRootfs(tarPath, rootfs)) {
        emit installFinished(false, "rootfs解压失败");
        return;
    }

    // 清理压缩包
    QFile::remove(tarPath);

    emit installFinished(true, QString("容器 %1 安装成功").arg(tag));
}

void ContainerManager::buildProotArgs(const QString& rootfsPath, QStringList& args)
{
    args.clear();
    args << "proot"
         << "--link2symlink"
         << "--kill-on-exit"
         << "-0"
         << "-r" << rootfsPath
         << "-b" << "/dev"
         << "-b" << "/proc"
         << "-w" << "/root"
         << "/usr/bin/env"
         << "-i"
         << "HOME=/root"
         << "PATH=/usr/local/sbin:/usr/local/bin:/bin:/usr/bin:/sbin:/usr/sbin"
         << "TERM=xterm-256color"
         << "LANG=C.UTF-8";

    // 选择shell
    QString bash = rootfsPath + "/usr/bin/bash";
    if (QFile::exists(bash)) {
        args << "/bin/bash" << "--login";
    } else {
        args << "/bin/sh";
    }
}

bool ContainerManager::startContainer(const QString& tag, qint64 ptyMasterFd)
{
    QString rootfs = QString::fromStdString(PboxPaths::rootfsDir(tag.toStdString()));
    if (!QDir(rootfs).exists()) {
        emit logMessage(QString("容器未安装: %1").arg(tag));
        return false;
    }

    QStringList args;
    buildProotArgs(rootfs, args);

    // 设置PROOT_LOADER环境变量
    QString loader = QString::fromStdString(PboxPaths::prootLoader());
    if (QFile::exists(loader)) {
        setenv("PROOT_LOADER", loader.toUtf8().constData(), 1);
    }

    QString proot = QString::fromStdString(PboxPaths::prootBin());
    emit logMessage(QString("启动: %1").arg(proot));

    // fork子进程，使用已有的pty master
    pid_t pid = fork();
    if (pid == 0) {
        // 子进程：将pty master复制为stdin/stdout/stderr
        dup2(ptyMasterFd, STDIN_FILENO);
        dup2(ptyMasterFd, STDOUT_FILENO);
        dup2(ptyMasterFd, STDERR_FILENO);

        // 构建argv
        QByteArray prog = proot.toUtf8();
        QVector<QByteArray> argStorage;
        QVector<char*> argv;
        argStorage.append(prog);
        argv.append(argStorage.last().data());
        for (const QString& a : args) {
            argStorage.append(a.toUtf8());
            argv.append(argStorage.last().data());
        }
        argv.append(nullptr);

        execv(prog.constData(), argv.data());
        _exit(127);
    }

    if (pid < 0) {
        emit logMessage("fork失败");
        return false;
    }

    emit logMessage(QString("容器已启动, pid=%1").arg(pid));
    return true;
}

bool ContainerManager::removeContainer(const QString& tag)
{
    QString rootfs = QString::fromStdString(PboxPaths::rootfsDir(tag.toStdString()));
    QDir dir(rootfs);
    if (!dir.exists()) return true;

    emit logMessage(QString("删除容器: %1").arg(tag));
    bool ok = dir.removeRecursively();
    if (ok) {
        emit logMessage("删除成功");
    } else {
        emit logMessage("删除失败");
    }
    return ok;
}

QString ContainerManager::prootVersion()
{
    QString proot = QString::fromStdString(PboxPaths::prootBin());
    QProcess proc;
    proc.start(proot, {"--version"});
    proc.waitForFinished(3000);
    return QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
}
