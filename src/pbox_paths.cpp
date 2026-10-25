// pbox_paths.cpp - 路径管理实现
#include "pbox_paths.h"
#include <QStandardPaths>
#include <QDir>
#include <QCoreApplication>
#include <QFileInfo>

namespace PboxPaths {

static QString s_rootOverride; // 测试用根目录覆盖

QString appFilesDir()
{
    if (!s_rootOverride.isEmpty())
        return s_rootOverride;
    // Android: /data/user/0/<pkg>/files ; 桌面: ~/.local/share/Pbox
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (base.isEmpty())
        base = QDir::homePath() + "/.pbox";
    QDir().mkpath(base);
    return base;
}

QString cacheDir()
{
    QString d = appFilesDir() + "/cache";
    QDir().mkpath(d);
    return d;
}

QString logDir()
{
    QString d = appFilesDir() + "/logs";
    QDir().mkpath(d);
    return d;
}

QString rootfsRoot()
{
    QString d = appFilesDir() + "/rootfs";
    QDir().mkpath(d);
    return d;
}

QString rootfsDir(const QString& tag)
{
    QString safe = tag;
    for (auto& c : safe) {
        if (!(c.isLetterOrNumber() || c == '-' || c == '_'))
            c = '_';
    }
    return rootfsRoot() + "/" + safe;
}

QString prootBin()
{
    return appFilesDir() + "/proot";
}

QString prootAssetPath()
{
    return "assets:/proot";
}

void ensureDirs()
{
    QDir().mkpath(appFilesDir());
    QDir().mkpath(cacheDir());
    QDir().mkpath(logDir());
    QDir().mkpath(rootfsRoot());
}

void setRootOverrideForTests(const QString& path)
{
    s_rootOverride = path;
}

} // namespace PboxPaths
