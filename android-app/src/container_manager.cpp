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

void ContainerManager::setPtraceAvailable(bool ok)
{
    if (m_ptraceAvailable == ok) return;
    m_ptraceAvailable = ok;
    emit ptraceAvailableChanged();
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
        QVariant v;
        v.setValue(c);
        result.append(v);
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

    QString proot = QString::fromStdString(PboxPaths::prootBin());

    QStringList args;
    if (tarPath.endsWith(".zst"))
        args << "--zstd";
    else if (tarPath.endsWith(".xz"))
        args << "-J";
    else if (tarPath.endsWith(".gz"))
        args << "-z";

    args << "-xf" << tarPath
         << "-C" << rootfsDir
         << "--preserve-permissions"
         << "--exclude=dev/*"
         << "--exclude=proc/*"
         << "--exclude=sys/*";

    QStringList fullArgs;
    fullArgs << "--link2symlink" << "tar";
    fullArgs.append(args);

    QProcess proc;
    proc.setProgram(proot);
    proc.setArguments(fullArgs);

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QString loader = QString::fromStdString(PboxPaths::prootLoader());
    if (QFile::exists(loader))
        env.insert("PROOT_LOADER", loader);
    // Android linker 不自动搜索 nativeLibDir，必须显式指定
    env.insert("LD_LIBRARY_PATH", QString::fromStdString(PboxPaths::nativeLibDir()));
    proc.setProcessEnvironment(env);

    proc.setProcessChannelMode(QProcess::MergedChannels);
    connect(&proc, &QProcess::readyReadStandardOutput, this, [&]() {
        emit logMessage(QString::fromUtf8(proc.readAllStandardOutput()));
    });

    proc.start();
    proc.waitForFinished(-1);

    if (proc.exitCode() != 0) {
        emit logMessage(QString("解压失败，退出码 %1").arg(proc.exitCode()));
        return false;
    }
    return true;
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

QString ContainerManager::pickShell(const QString& rootfsPath)
{
    QStringList candidates = {"/usr/bin/bash", "/bin/bash", "/usr/bin/sh", "/bin/sh"};
    for (const QString& c : candidates)
        if (QFile::exists(rootfsPath + c)) return c;
    return "/bin/sh";
}

void ContainerManager::buildProotArgs(const QString& rootfsPath, QStringList& args)
{
    args.clear();
    args << "--link2symlink"
         << "--kill-on-exit"
         << "-0"
         << "-r" << rootfsPath
         << "-b" << "/dev"
         << "-b" << "/proc"
         << "-b" << "/sys"
         << "-b" << "/dev/null"
         << "-w" << "/root";

    QString sdcard = "/storage/emulated/0";
    if (QFile::exists(sdcard))
        args << "-b" << sdcard + ":/sdcard";

    args << "/usr/bin/env"
         << "-i"
         << "HOME=/root"
         << "TERM=xterm-256color"
         << "LANG=C.UTF-8"
         << "PATH=/usr/local/sbin:/usr/local/bin:/bin:/usr/bin:/sbin:/usr/sbin"
         << "USER=root";

    args << pickShell(rootfsPath) << "--login";
}

bool ContainerManager::startContainer(const QString& tag, qint64 ptyMasterFd)
{
    if (!m_ptraceAvailable) {
        emit logMessage("错误: 当前环境禁止 ptrace，proot 无法运行。"
                        "（卓易通/鸿蒙兼容层通常限制 ptrace）");
        return false;
    }

    QString rootfs = QString::fromStdString(PboxPaths::rootfsDir(tag.toStdString()));
    if (!QDir(rootfs).exists()) {
        emit logMessage("容器未安装: " + tag);
        return false;
    }

    QString proot = QString::fromStdString(PboxPaths::prootBin());
    QString loader = QString::fromStdString(PboxPaths::prootLoader());

    if (!QFile::exists(proot)) {
        emit logMessage("错误: 未找到内置 proot: " + proot);
        return false;
    }
    if (!QFile::exists(loader)) {
        emit logMessage("错误: 未找到 proot loader: " + loader);
        return false;
    }

    QStringList args;
    buildProotArgs(rootfs, args);

    emit logMessage("启动容器: " + tag);
    qInfo() << "proot:" << proot << " loader:" << loader;
    qInfo() << "args:" << args.join(' ');

    pid_t pid = fork();
    if (pid == 0) {
        dup2(ptyMasterFd, STDIN_FILENO);
        dup2(ptyMasterFd, STDOUT_FILENO);
        dup2(ptyMasterFd, STDERR_FILENO);
        setenv("PROOT_LOADER", loader.toUtf8().constData(), 1);
        // Android linker 不自动搜索 nativeLibDir，必须显式指定
        setenv("LD_LIBRARY_PATH",
               QString::fromStdString(PboxPaths::nativeLibDir()).toUtf8().constData(), 1);

        QVector<QByteArray> storage;
        QVector<char*> argv;
        storage.append(proot.toUtf8());
        argv.append(storage.last().data());
        for (const QString& a : args) {
            storage.append(a.toUtf8());
            argv.append(storage.last().data());
        }
        argv.append(nullptr);
        execv(proot.toUtf8().constData(), argv.data());
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
    QString proot = QString::fromStdString(PboxPaths::prootBin());
    if (!QFile::exists(proot)) return "未找到 proot";
    QProcess proc;
    proc.start(proot, {"--version"});
    proc.waitForFinished(3000);
    return QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
}
