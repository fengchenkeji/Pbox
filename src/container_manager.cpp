// container_manager.cpp - 容器管理核心实现
#include "container_manager.h"
#include "pbox_paths.h"

#include <QDebug>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QFileInfo>
#include <QDir>
#include <QMap>
#include <QEventLoop>
#include <QStorageInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QJSEngine>
#include <QQmlEngine>

#include <unistd.h>
#include <sys/utsname.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <cstdlib>
#include <cstring>

ContainerManager* ContainerManager::create(QQmlEngine*, QJSEngine*)
{
    return &ContainerManager::instance();
}

ContainerManager& ContainerManager::instance()
{
    static ContainerManager inst;
    return inst;
}

ContainerManager::ContainerManager(QObject* parent)
    : QObject(parent)
{
    m_nam = new QNetworkAccessManager(this);
    PboxPaths::ensureDirs();
    m_prootReady = QFileInfo::exists(PboxPaths::prootBin());
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
    QString qarch = QSysInfo::currentCpuArchitecture();
    if (qarch == "arm64" || qarch == "aarch64") return "arm64";
    if (qarch.startsWith("arm")) return "armhf";
    if (qarch == "x86_64" || qarch == "amd64") return "amd64";
    if (qarch == "riscv64") return "riscv64";

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
    static const QMap<QString, QStringList> releases = {
        {"ubuntu",    {"noble", "jammy", "focal", "mantic", "oracular"}},
        {"debian",    {"trixie", "bookworm", "bullseye"}},
        {"alpine",    {"3.20", "3.19", "3.18", "edge"}},
        {"fedora",    {"40", "39", "41"}},
        {"archlinux", {"latest"}},
        {"kali",      {"current"}},
    };
    return releases.value(os.toLower(), {});
}

QVariantList ContainerManager::listInstalled()
{
    QVariantList result;
    QDir rootfsRoot(PboxPaths::rootfsRoot());
    for (const auto& entry : rootfsRoot.entryInfoList(QDir::AllDirs | QDir::NoDotAndDotDot)) {
        QString tag = entry.fileName();
        QVariantMap m;
        m["tag"] = tag;
        QStringList parts = tag.split('_');
        if (parts.size() >= 2) {
            m["os"] = parts[0];
            m["release"] = parts.mid(1).join("_");
        } else {
            m["os"] = tag;
            m["release"] = "";
        }
        m["rootfsPath"] = entry.absoluteFilePath();
        m["sizeBytes"] = dirSizeRecursive(entry.absoluteFilePath());
        result.append(m);
    }
    return result;
}

qint64 ContainerManager::dirSizeRecursive(const QString& path)
{
    QDir dir(path);
    qint64 size = 0;
    const auto files = dir.entryInfoList(QDir::Files | QDir::Hidden);
    for (const auto& fi : files)
        size += fi.size();
    const auto subdirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden);
    for (const auto& d : subdirs)
        size += dirSizeRecursive(d.absoluteFilePath());
    return size;
}

QString ContainerManager::formatSize(qint64 bytes)
{
    if (bytes < 1024) return QString("%1 B").arg(bytes);
    if (bytes < 1024 * 1024) return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    if (bytes < 1024LL * 1024 * 1024)
        return QString("%1 MB").arg(bytes / (1024.0 * 1024), 0, 'f', 1);
    return QString("%1 GB").arg(bytes / (1024.0 * 1024 * 1024), 0, 'f', 2);
}

QStringList ContainerManager::getMirrorUrls(const QString& os, const QString& release, const QString& arch)
{
    // 多个镜像源，失败自动切换
    QStringList bases = {
        "https://mirrors.tuna.tsinghua.edu.cn/lxc-images",
        "https://mirror.nyist.edu.cn/lxc-images",
        "https://mirrors.aliyun.com/lxc-images",
        "https://mirrors.163.com/lxc-images",
        "https://images.linuxcontainers.org",
    };
    // LXC 官方镜像的架构命名与 termux 不同：arm64 用 arm64，armhf 用 armhf，amd64 用 amd64
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
    m_rootfsDir = PboxPaths::rootfsDir(tag);
    m_savePath = PboxPaths::cacheDir() + "/" + tag + ".tar.zst";

    emit logMessage(QString("准备安装 %1:%2 (%3)").arg(m_os, m_release, m_arch));

    ensureStorageAvailable();
    if (m_cancelled) return;

    m_mirrorUrls = getMirrorUrls(m_os, m_release, m_arch);
    m_mirrorIndex = 0;
    tryNextMirror();
}

void ContainerManager::ensureStorageAvailable()
{
    QStorageInfo storage(PboxPaths::appFilesDir());
    if (storage.bytesAvailable() < 2LL * 1024 * 1024 * 1024) {
        emit installFinished(false, "存储空间不足 2GB，无法安装");
        m_cancelled = true;
    }
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
    emit logMessage(QString("尝试镜像 [%1/%2]").arg(m_mirrorIndex + 1).arg(m_mirrorUrls.size()));

    if (m_file) { m_file->close(); delete m_file; m_file = nullptr; }
    QDir().mkpath(QFileInfo(m_savePath).absolutePath());
    m_file = new QFile(m_savePath, this);
    if (!m_file->open(QIODevice::WriteOnly)) {
        emit installFinished(false, "无法创建下载文件");
        return;
    }

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "Pbox/1.1 (Android)");
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
        double speed = m_timer.elapsed() > 0 ? (mb / (m_timer.elapsed() / 1000.0)) : 0;
        emit downloadProgress(pct, QString("下载 %1/%2 MB - %3 MB/s")
            .arg(mb, 0, 'f', 1).arg(totalMb, 0, 'f', 1).arg(speed, 0, 'f', 1));
    } else {
        emit downloadProgress(0, QString("已下载 %1 MB").arg(received / 1024.0 / 1024.0, 0, 'f', 1));
    }
}

void ContainerManager::onDownloadFinished()
{
    if (!m_reply) return;
    if (m_reply->error() != QNetworkReply::NoError) {
        onDownloadError(m_reply->error());
        return;
    }
    m_file->write(m_reply->readAll());
    m_file->close();
    m_reply->deleteLater();
    m_reply = nullptr;

    qint64 size = QFileInfo(m_savePath).size();
    emit logMessage(QString("下载完成，共 %1 MB").arg(size / 1024.0 / 1024.0, 0, 'f', 1));

    if (size < 100 * 1024) {
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
        emit installFinished(true, QString("容器 %1 安装成功").arg(m_os + "_" + m_release));
    } else {
        QFile::remove(m_savePath);
        emit installFinished(false, "rootfs 解压失败");
    }
}

void ContainerManager::onDownloadError(QNetworkReply::NetworkError code)
{
    Q_UNUSED(code);
    QString err = m_reply ? m_reply->errorString() : "未知错误";
    if (m_reply) { m_reply->deleteLater(); m_reply = nullptr; }
    if (m_file) { m_file->close(); delete m_file; m_file = nullptr; }
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

    // Android 8~15 自带 toybox tar，压缩格式支持度不同，依次尝试
    QStringList candidates;
    if (tarPath.endsWith(".zst"))      candidates = {"--zstd", "-J", "-z", ""};
    else if (tarPath.endsWith(".xz"))  candidates = {"-J", "--zstd", "-z", ""};
    else                               candidates = {"-z", ""};

    QString tarBin = QFile::exists("/system/bin/tar") ? "/system/bin/tar" : "tar";

    for (const QString& flag : candidates) {
        QStringList args;
        if (!flag.isEmpty()) args << flag;
        args << "-xf" << tarPath << "-C" << rootfsDir
             << "--exclude=dev/*" << "--exclude=proc/*" << "--exclude=sys/*";

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

// 修复 resolv.conf 与 usrmerge 软链接
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

    // usrmerge：把根层的 bin/sbin/lib/lib64 链接到 usr 下对应目录
    QDir dir(rootfsDir);
    QStringList links = {"bin", "sbin", "lib", "lib64", "usr/bin", "usr/sbin", "usr/lib", "usr/lib64"};
    for (int i = 0; i < 4; ++i) {
        const QString& l = links[i];
        QString usrPath = dir.filePath("usr/" + l);
        QString linkPath = dir.filePath(l);
        if (QFile::exists(usrPath) && !QFile::exists(linkPath)) {
            QFile::link(usrPath, linkPath);
            emit logMessage("usrmerge 软链接: " + l + " -> usr/" + l);
        }
    }
    emit logMessage("rootfs 修复完成（resolv.conf + usrmerge）");
}

bool ContainerManager::removeContainer(const QString& tag)
{
    QDir dir(PboxPaths::rootfsDir(tag));
    if (!dir.exists()) return true;
    emit logMessage("删除容器: " + tag);
    return dir.removeRecursively();
}

// ===== proot 二进制 =====
QString ContainerManager::ensureProot()
{
    if (m_prootReady && QFileInfo::exists(PboxPaths::prootBin()))
        return PboxPaths::prootBin();

    // 1. 尝试从打包进 APK 的 assets 复制（Android 主路径）
    if (installProotFromAssets()) {
        m_prootReady = true;
        return PboxPaths::prootBin();
    }
    // 2. 尝试从 Termux 仓库网络下载（桌面调试/兜底）
    emit logMessage("未找到内置 proot，尝试从 Termux 仓库下载...");
    if (installProotFromNetwork()) {
        m_prootReady = true;
        return PboxPaths::prootBin();
    }
    emit logMessage("proot 不可用，将降级为系统 shell 终端");
    return QString();
}

bool ContainerManager::installProotFromAssets()
{
    const QStringList assetPaths = {"assets:/proot", "qrc:/proot"};
    for (const QString& p : assetPaths) {
        QFile in(p);
        if (!in.exists() || !in.open(QIODevice::ReadOnly)) continue;
        QString out = PboxPaths::prootBin();
        QFile of(out);
        if (of.open(QIODevice::WriteOnly)) {
            of.write(in.readAll());
            of.close();
            ::chmod(out.toUtf8().constData(), 0755);
            emit logMessage("proot 已从内置资源安装: " + out);
            return true;
        }
    }
    return false;
}

bool ContainerManager::installProotFromNetwork()
{
    const QString arch = detectNativeArch();
    const QString abi = arch == "arm64" ? "arm64" : (arch == "amd64" ? "x86_64" : "arm");

    // Termux 仓库 proot deb 包，版本号优先尝试 5.4.0，其次 5.3.0 / 5.2.0
    QStringList versions = {"5.4.0", "5.3.0", "5.2.0", "5.1.1"};
    QStringList urls;
    for (const QString& v : versions)
        urls << QString("https://packages.termux.dev/apt/termux-main/pool/main/p/proot/proot_%1_%2.deb")
                .arg(v, abi);

    const QString debPath = PboxPaths::cacheDir() + "/proot.deb";
    const QString tmp = PboxPaths::cacheDir() + "/proot_extract";

    for (const QString& url : urls) {
        QNetworkRequest req(url);
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
        QNetworkReply* rep = m_nam->get(req);
        QEventLoop loop;
        connect(rep, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();
        if (rep->error() != QNetworkReply::NoError) { rep->deleteLater(); continue; }
        QFile f(debPath);
        if (f.open(QIODevice::WriteOnly)) {
            f.write(rep->readAll());
            f.close();
        }
        rep->deleteLater();

        // 解析 deb（ar 归档 -> data.tar.xz -> usr/bin/proot）
        QDir().mkpath(tmp);
        QDir(tmp).removeRecursively();
        QDir().mkpath(tmp);
        QProcess ar;
        ar.start("ar", {"x", debPath}, QIODevice::ReadOnly);
        if (!ar.waitForFinished(30000)) continue;
        // 找到 data.tar.*
        QString dataTar;
        QDir d(tmp);
        for (const auto& fi : d.entryInfoList(QDir::Files)) {
            if (fi.fileName().startsWith("data.tar.")) { dataTar = fi.fileName(); break; }
        }
        if (dataTar.isEmpty()) continue;
        QProcess tar;
        tar.start("tar", {"-xf", tmp + "/" + dataTar, "-C", tmp}, QIODevice::ReadOnly);
        if (!tar.waitForFinished(120000)) continue;
        QString bin = tmp + "/data/data/com.termux/files/usr/bin/proot";
        if (!QFile::exists(bin)) continue;
        if (QFile::copy(bin, PboxPaths::prootBin())) {
            ::chmod(PboxPaths::prootBin().toUtf8().constData(), 0755);
            emit logMessage("proot 已从网络下载安装: " + PboxPaths::prootBin());
            QDir(tmp).removeRecursively();
            return true;
        }
        QDir(tmp).removeRecursively();
        break;
    }
    return false;
}

QString ContainerManager::prootVersion()
{
    if (m_prootReady || QFileInfo::exists(PboxPaths::prootBin()))
        return "proot 已就绪";
    return "未就绪（启动时自动获取或降级）";
}

// 生成 proot 启动命令
QStringList ContainerManager::prootCommand(const QString& tag)
{
    QString proot = PboxPaths::prootBin();
    if (!QFileInfo::exists(proot))
        proot = ensureProot();
    if (proot.isEmpty() || !QFileInfo::exists(proot)) {
        emit logMessage("proot 不可用，无法隔离运行");
        return {};
    }
    QString rootfs = PboxPaths::rootfsDir(tag);
    if (!QDir(rootfs).exists()) {
        emit logMessage("容器 rootfs 不存在: " + rootfs);
        return {};
    }

    QStringList args;
    args << proot
         << "--link2symlink"
         << "-0"
         << "-r" << rootfs
         << "-b" << "/dev"
         << "-b" << "/proc"
         << "-b" << "/sys"
         << "-b" << "/data"
         << "-w" << "/"
         << "/bin/sh";
    return args;
}
