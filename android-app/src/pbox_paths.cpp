// pbox_paths.cpp - Pbox Android 路径管理实现
#include "pbox_paths.h"
#include <QStandardPaths>
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#ifdef Q_OS_ANDROID
#include <QJniObject>
#include <QJniEnvironment>
#endif
#include <sys/stat.h>
#include <cstdlib>

std::string PboxPaths::s_appFilesDir;
std::string PboxPaths::s_nativeLibDir;
bool PboxPaths::s_initialized = false;

void PboxPaths::initialize()
{
    if (s_initialized) return;

    QString filesPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    s_appFilesDir = filesPath.toStdString();

#ifdef Q_OS_ANDROID
    QJniObject activity = QJniObject::callStaticMethod<jobject>(
        "org/qtproject/qt/android/QtNative",
        "getActivity"
    );
    if (activity.isValid()) {
        QJniObject appInfo = activity.callMethod<jobject>("getApplicationInfo");
        if (appInfo.isValid()) {
            QJniObject libDir = appInfo.getObjectField(
                "nativeLibraryDir", "Ljava/lang/String;");
            if (libDir.isValid()) {
                s_nativeLibDir = libDir.toString().toStdString();
            }
        }
    }
#endif

    if (s_nativeLibDir.empty()) {
        QString dir = QCoreApplication::applicationDirPath();
        s_nativeLibDir = dir.toStdString();
        qWarning() << "JNI获取nativeLibraryDir失败，使用:" << QString::fromStdString(s_nativeLibDir);
    }

    s_initialized = true;
    qInfo() << "PboxPaths:";
    qInfo() << "  files:" << QString::fromStdString(s_appFilesDir);
    qInfo() << "  libs:" << QString::fromStdString(s_nativeLibDir);
}

std::string PboxPaths::appFilesDir() { return s_appFilesDir; }
std::string PboxPaths::nativeLibDir() { return s_nativeLibDir; }

std::string PboxPaths::prootBin()
{
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

std::string PboxPaths::cacheDir()
{
    return s_appFilesDir + "/cache";
}

std::string PboxPaths::logDir()
{
    return s_appFilesDir + "/logs";
}

std::string PboxPaths::scriptsDir()
{
    return s_appFilesDir + "/scripts";
}

void PboxPaths::ensureDirs()
{
    auto mkdir = [](const std::string& path) {
        std::error_code ec;
        fs::create_directories(path, ec);
        if (ec)
            qWarning() << "创建目录失败:" << QString::fromStdString(path) << ec.message().c_str();
    };
    mkdir(rootfsRoot());
    mkdir(scriptsDir());
    mkdir(cacheDir());
    mkdir(logDir());
}
