// container_manager.cpp - 容器管理核心实现
#include "container_manager.h"
#include "pbox_paths.h"

#include <QDebug>
#include <QNetworkRequest>
#include <QFileInfo>
#include <QDir>
#include <QStorageInfo>
#include <QProcess>
#include <QProcessEnvironment>

#include <unistd.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstdlib>
#include <cstring>

ContainerManager& ContainerManager::instance()
{
    static ContainerManager inst;
    return inst;
}

ContainerManager::ContainerManager(QObject* parent)
    : QObject(parent)
{
    m_nam = new QNetworkAccessManager(this);
    qRegisterMetaType<InstalledContainer>();
}

QString ContainerManager::logPath() const
{
    return QString::fromStdString(PboxPaths::logDir());
}

QString ContainerManager::arch()
{
    return detectNativeArch();
}

QString ContainerManager::detectNativeArch()
{
    struct utsname u;
    if (uname(&u) == 0) {
        QString machine(u.machine);
        if (machine == "aarch64" || machine == "arm64") return "arm64";
        if (machine.startsWith("arm")) return "armhf";
        if (machine == "x86_64" || machine == "amd64") return "amd64";
        if (machine == "riscv64") return "riscv64";
        if (!machine.isEmpty()) return machine;
    }
    // uname 失败/异常时用 QSysInfo 兜底
    QString qarch = QSysInfo::currentCpuArchitecture();
    if (qarch == "arm64" || qarch == "aarch64") return "arm64";
    if (qarch.startsWith("arm")) return "armhf";
    if (qarch == "x86_64" || qarch == "amd64") return "amd64";
    if (qarch == "riscv64") return "riscv64";
    // 最后兜底
    QFile f("/proc/cpuinfo");
    if (f.open(QIODevice::ReadOnly)) {
        QString cpu = QString::fromUtf8(f.readAll());
        if (cpu.contains("aarch64")) return "arm64";
        if (cpu.contains("ARMv")) return "armhf";
        if (cpu.contains("GenuineIntel") || cpu.contains("AuthenticAMD")) return "amd64";
    }
    return "arm64";
}

QStringList ContainerManager::listAvailableOS()
{
    return {"ubuntu", "debian", "alpine", "fedora", "archlinux", "kali"};
}

QStringList ContainerManager::listReleases(const QString& os)
{
    QMap<QString, QStringList> releases = {
        {"ubuntu",   {"noble", "jammy", "focal", "mantic", "oracular"}},
        {"debian",   {"trixie", "bookworm", "bullseye"}},
        {"alpine",   {"3.20", "3.19", "edge"}},
        {"fedora",   {"40", "39"}},
        {"archlinux",{"latest"}},
        {"kali",     {"current"}}
    };
    return releases.value(os.toLower(), {});
}

QVariantList ContainerManager::listInstalled()
{
    QVariantList result;
    QDir rootfsRoot(QString::fromStdString(PboxPaths::rootfsRoot()));

    for (const auto& entry : rootfsRoot.entryInfoList(QDir::AllDirs | QDir::NoDotAndDotDot)) {
        InstalledContainer c;
        c.tag = entry.fileName();
        c.rootfsPath = entry.absoluteFilePath();
        QStringList parts = c.tag.split('_');
        if (parts.size() >= 2) {
            c.os = parts[0];
            c.release = parts.mid(1).join("_");
        }
        c.sizeBytes = dirSize(entry.absoluteFilePath());
        // 用 QVariantMap 暴露给 QML，避免 Q_GADGET 属性访问兼容问题
        QVariantMap m;
        m["tag"] = c.tag;
        m["os"] = c.os;
        m["release"] = c.release;
        m["rootfsPath"] = c.rootfsPath;
        m["sizeBytes"] = c.sizeBytes;
        result.append(m);
    }
    return result;
}

qint64 ContainerManager::dirSize(const QString& path)
{
    QDir dir(path);
    qint64 size = 0;
    for (const auto& fi : dir.entryInfoList(QDir::Files | QDir::Hidden))
        size += fi.size();
    return size;
}

QString ContainerManager::formatSize(qint64 bytes)
{
    if (bytes < 1024) return QString("%1 B").arg(bytes);
    if (bytes < 1024*1024) return QString("%1 KB").arg(bytes/1024.0, 0, 'f', 1);
    if (bytes < 1024LL*1024*1024) return QString("%1 MB").arg(bytes/(1024.0*1024), 0, 'f', 1);
    return QString("%1 GB").arg(bytes/(1024.0*1024*1024), 0, 'f', 2);
}

QStringList ContainerManager::getMirrorUrls(const QString& os, const QString& release, const QString& arch)
{
    QStringList bases = {
        "https://mirrors.tuna.tsinghua.edu.cn/lxc-images",
        "https://mirror.nyist.edu.cn/lxc-images",
        "https://mirrors.aliyun.com/lxc-images",
        "https://mirrors.163.com/lxc-images",
        "https://images.linuxcontainers.org"
    };
    QString sub = QString("images/%1/%2/%3/default/").arg(os.toLower(), release.toLower(), arch);

    QStringList urls;
    for (const QString& base : bases)
        urls << base + "/" + sub + "rootfs.tar.zst";
    return urls;
}

void ContainerManager::installContainer(const QString& os, const QString& release)
{
    m_os = os.toLower();
    m_release = release.toLower();
    m_arch = detectNativeArch();
    m_cancelled = false;

    QString tag = QString("%1_%2").arg(m_os, m_release);
    m_rootfsDir = QString::fromStdString(PboxPaths::rootfsDir(tag.toStdString()));
    m_savePath = QString::fromStdString(PboxPaths::cacheDir()) + "/" + tag + ".tar.zst";

    emit logMessage(QString("准备安装 %1:%2 (%3)").arg(m_os, m_release, m_arch));

    QStorageInfo storage(QString::fromStdString(PboxPaths::appFilesDir()));
    if (storage.bytesAvailable() < 2LL * 1024 * 1024 * 1024) {
        emit installFinished(false, "存储空间不足2GB，无法安装");
        return;
    }

    m_mirrorUrls = getMirrorUrls(m_os, m_release, m_arch);
    m_mirrorIndex = 0;
    tryNextMirror();
}

void ContainerManager::tryNextMirror()
{
    if (m_cancelled) {
        emit installFinished(false, "已取消");
        return;
    }
    if (m_mirrorIndex >= m_mirrorUrls.size()) {
        emit installFinished(false, "所有镜像源均下载失败");
        return;
    }

    QString url = m_mirrorUrls[m_mirrorIndex];
    emit logMessage(QString("尝试镜像 [%1/%2]").arg(m_mirrorIndex+1).arg(m_mirrorUrls.size()));

    if (m_file) {
        m_file->close();
        delete m_file;
        m_file = nullptr;
    }

    QDir().mkpath(QFileInfo(m_savePath).absolutePath());
    m_file = new QFile(m_savePath, this);
    if (!m_file->open(QIODevice::WriteOnly)) {
        emit installFinished(false, "无法创建下载文件");
        return;
    }

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "Pbox/1.0 (Android)");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                        QNetworkRequest::NoLessSafeRedirectPolicy);

    m_timer.restart();
    m_reply = m_nam->get(request);
    connect(m_reply, &QNetworkReply::downloadProgress,
            this, &ContainerManager::onDownloadProgress);
    connect(m_reply, &QNetworkReply::finished,
            this, &ContainerManager::onDownloadFinished);
    connect(m_reply, &QNetworkReply::errorOccurred,
            this, &ContainerManager::onDownloadError);
}

void ContainerManager::onDownloadProgress(qint64 received, qint64 total)
{
    if (total > 0) {
        double pct = received * 100.0 / total;
        double mb = received / 1024.0 / 1024.0;
        double totalMb = total / 1024.0 / 1024.0;
        qint64 elapsed = m_timer.elapsed();
        double speed = elapsed > 0 ? (mb / (elapsed / 1000.0)) : 0;
        emit downloadProgress(pct,
            QString("下载 %1/%2 MB - %3 MB/s")
                .arg(mb, 0, 'f', 1).arg(totalMb, 0, 'f', 1).arg(speed, 0, 'f', 1));
    } else {
        emit downloadProgress(0, QString("已下载 %1 MB").arg(received/1024.0/1024.0, 0, 'f', 1));
    }
}

void ContainerManager::onDownloadFinished()
{
    if (m_reply->error() != QNetworkReply::NoError) {
        onDownloadError(m_reply->error());
        return;
    }

    m_file->write(m_reply->readAll());
    m_file->close();
    m_reply->deleteLater();
    m_reply = nullptr;

    qint64 size = QFileInfo(m_savePath).size();
    emit logMessage(QString("下载完成，共 %1 MB").arg(size/1024.0/1024.0, 0, 'f', 1));

    if (size < 1024 * 100) {
        emit logMessage("文件过小，切换下一镜像");
        m_mirrorIndex++;
        tryNextMirror();
        return;
    }

    emit logMessage("开始解压 rootfs ...");
    emit downloadProgress(0, "正在解压...");

    if (extractRootfs(m_savePath, m_rootfsDir)) {
        fixRootfs(m_rootfsDir);
        QFile::remove(m_savePath);
        emit installFinished(true, QString("容器 %1 安装成功").arg(m_os+"_"+m_release));
    } else {
        QFile::remove(m_savePath);
        emit installFinished(false, "rootfs 解压失败");
    }
}

void ContainerManager::onDownloadError(QNetworkReply::NetworkError code)
{
    Q_UNUSED(code);
    QString err = m_reply->errorString();
    m_reply->deleteLater();
    m_reply = nullptr;
    m_file->close();
    delete m_file;
    m_file = nullptr;
    QFile::remove(m_savePath);

    emit logMessage("镜像失败: " + err);
    m_mirrorIndex++;
    tryNextMirror();
}

void ContainerManager::cancelDownload()
{
    m_cancelled = true;
    if (m_reply) m_reply->abort();
}

bool ContainerManager::extractRootfs(const QString& tarPath, const QString& rootfsDir)
{
    QDir().mkpath(rootfsDir);

    // Android 8~17 自带 toybox tar，支持 gzip/xz/zstd（版本不同支持度不同），依次尝试
    QStringList candidates;
    if (tarPath.endsWith(".zst"))      candidates = {"--zstd", "-J", "-z", ""};
    else if (tarPath.endsWith(".xz"))  candidates = {"-J", "--zstd", "-z", ""};
    else                               candidates = {"-z", ""};

    QString tarBin = QFile::exists("/system/bin/tar") ? "/system/bin/tar" : "tar";

    for (const QString& flag : candidates) {
        QStringList args;
        if (!flag.isEmpty()) args << flag;
        args << "-xf" << tarPath << "-C" << rootfsDir
             << "--exclude=dev/*"
             << "--exclude=proc/*"
             << "--exclude=sys/*";

        QProcess proc;
        proc.setProgram(tarBin);
        proc.setArguments(args);
        proc.setProcessChannelMode(QProcess::MergedChannels);
        connect(&proc, &QProcess::readyReadStandardOutput, this, [&]() {
            emit logMessage(QString::fromUtf8(proc.readAllStandardOutput()));
        });

        emit logMessage("解压: tar " + args.join(' '));
        proc.start();
        if (!proc.waitForFinished(-1)) continue;

        if (proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0) {
            emit logMessage("rootfs 解压完成");
            return true;
        }
        emit logMessage(QString("解压方式 %1 失败，尝试其他方式...")
                        .arg(flag.isEmpty() ? "auto" : flag));
    }

    emit logMessage("错误: rootfs 解压失败（系统 tar 不支持该压缩格式）");
    return false;
}

void ContainerManager::fixRootfs(const QString& rootfsDir)
{
    QString resolv = rootfsDir + "/etc/resolv.conf";
    QFile::remove(resolv);
    QFile f(resolv);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        f.write("nameserver 114.114.114.114\n");
        f.write("nameserver 223.5.5.5\n");
        f.write("nameserver 8.8.8.8\n");
    }

    QDir dir(rootfsDir);
    QStringList links = {"bin", "sbin", "lib", "lib64"};
    for (const QString& l : links) {
        QString linkPath = dir.filePath(l);
        QString usrPath = dir.filePath("usr/" + l);
        if (QFile::exists(usrPath) && !QFile::exists(linkPath))
            QFile::link(usrPath, linkPath);
    }
    emit logMessage("rootfs 修复完成");
}

bool ContainerManager::startContainer(const QString& tag, qint64 ptyMasterFd)
{
    QString rootfs = QString::fromStdString(PboxPaths::rootfsDir(tag.toStdString()));
    bool rootfsExists = QDir(rootfs).exists();

    // v1.0.3 无内置 proot：启动系统 shell 终端（图形化终端能力，容器文件可浏览）
    QString banner;
    if (rootfsExists) {
        banner = QStringLiteral(
            "\x1b[32mPbox 终端 v1.0.3\x1b[0m\n"
            "已安装容器: %1\n"
            "当前版本未内置 proot 运行环境，无法隔离运行 Linux 程序。\n"
            "已切换到容器 rootfs 目录，可用 ls / cd 浏览文件。\n"
            "支持 proot 的完整版将在后续提供。\n\n").arg(rootfs);
    } else {
        banner = QStringLiteral(
            "\x1b[32mPbox 终端 v1.0.3\x1b[0m\n"
            "未找到容器 %1，这是 Android 系统 shell。\n"
            "可执行系统命令（ls / pwd / echo 等）。\n\n").arg(rootfs);
    }
    ::write(ptyMasterFd, banner.toUtf8().constData(), banner.toUtf8().size());

    emit logMessage("启动终端: " + tag);

    pid_t pid = fork();
    if (pid == 0) {
        dup2(ptyMasterFd, STDIN_FILENO);
        dup2(ptyMasterFd, STDOUT_FILENO);
        dup2(ptyMasterFd, STDERR_FILENO);

        setenv("TERM", "xterm-256color", 1);
        setenv("HOME", "/data/data/com.pbox.app/files", 1);
        setenv("PATH", "/system/bin:/system/xbin:/sbin:/bin", 1);
        setenv("PS1", "pbox@android:/ \\$ ", 1);
        setenv("LANG", "C.UTF-8", 1);

        if (rootfsExists)
            chdir(rootfs.toUtf8().constData());

        execl("/system/bin/sh", "sh", nullptr);
        _exit(127);
    }

    if (pid < 0) {
        emit logMessage("fork 失败");
        return false;
    }
    emit containerStarted(tag);
    return true;
}

bool ContainerManager::removeContainer(const QString& tag)
{
    QString rootfs = QString::fromStdString(PboxPaths::rootfsDir(tag.toStdString()));
    QDir dir(rootfs);
    if (!dir.exists()) return true;
    emit logMessage("删除容器: " + tag);
    return dir.removeRecursively();
}

QString ContainerManager::prootVersion()
{
    return QStringLiteral("未内置（终端模式 v1.0.3）");
}
