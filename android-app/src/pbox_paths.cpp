// pbox_paths.cpp - Android App路径管理实现
#include "pbox_paths.h"
#include <QStandardPaths>
#include <QCoreApplication>
#include <QAndroidJniObject>
#include <QDebug>
#include <sys/stat.h>
#include <cstdlib>

std::string PboxPaths::s_appFilesDir;
std::string PboxPaths::s_nativeLibDir;
bool PboxPaths::s_initialized = false;

void PboxPaths::initialize()
{
    if (s_initialized) return;

    // 通过Qt获取App私有文件目录
    QString filesPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    // AppDataLocation 返回 /data/data/com.pbox.app/files
    s_appFilesDir = filesPath.toStdString();

    // 通过Android API获取nativeLibraryDir
    // Activity.getApplicationInfo().nativeLibraryDir
    QAndroidJniObject activity = QAndroidJniObject::callStaticObjectMethod(
        "org/qtproject/qt/android/QtNative",
        "getActivity",
        "()Landroid/app/Activity;"
    );
    if (activity.isValid()) {
        QAndroidJniObject appInfo = activity.callObjectMethod(
            "getApplicationInfo",
            "()Landroid/content/pm/ApplicationInfo;"
        );
        if (appInfo.isValid()) {
            QAndroidJniObject libDir = appInfo.getObjectField(
                "nativeLibraryDir",
                "Ljava/lang/String;"
            );
            if (libDir.isValid()) {
                s_nativeLibDir = libDir.toString().toStdString();
            }
        }
    }

    // 兜底：如果JNI获取失败，用常见路径猜测
    if (s_nativeLibDir.empty()) {
        qWarning() << "无法通过JNI获取nativeLibraryDir，使用默认路径";
        s_nativeLibDir = s_appFilesDir + "/lib";
    }

    s_initialized = true;
    qInfo() << "PboxPaths initialized:";
    qInfo() << "  appFilesDir:" << QString::fromStdString(s_appFilesDir);
    qInfo() << "  nativeLibDir:" << QString::fromStdString(s_nativeLibDir);
}

std::string PboxPaths::appFilesDir()
{
    return s_appFilesDir;
}

std::string PboxPaths::nativeLibDir()
{
    return s_nativeLibDir;
}

std::string PboxPaths::prootBin()
{
    // proot在APK中命名为libpbox_proot.so，放在nativeLibraryDir
    return s_nativeLibDir + "/libpbox_proot.so";
}

std::string PboxPaths::prootLoader()
{
    return s_nativeLibDir + "/libpbox_proot_loader.so";
}

std::string PboxPaths::tallocLib()
{
    return s_nativeLibDir + "/libtalloc.so";
}

std::string PboxPaths::rootfsRoot()
{
    return s_appFilesDir + "/rootfs";
}

std::string PboxPaths::rootfsDir(const std::string& tag)
{
    return rootfsRoot() + "/" + tag;
}

std::string PboxPaths::scriptDir()
{
    return s_appFilesDir + "/scripts";
}

std::string PboxPaths::cacheDir()
{
    return s_appFilesDir + "/cache";
}

std::string PboxPaths::logDir()
{
    return s_appFilesDir + "/logs";
}

void PboxPaths::ensureDirs()
{
    auto mkdir = [](const std::string& path) {
        std::error_code ec;
        fs::create_directories(path, ec);
        if (ec) {
            qWarning() << "创建目录失败:" << QString::fromStdString(path)
                       << ec.message().c_str();
        }
    };

    mkdir(rootfsRoot());
    mkdir(scriptDir());
    mkdir(cacheDir());
    mkdir(logDir());
}
