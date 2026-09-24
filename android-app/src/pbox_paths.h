// pbox_paths.h - Android App路径管理
#ifndef PBOX_PATHS_H
#define PBOX_PATHS_H

#include <string>
#include <filesystem>

namespace fs = std::filesystem;

class PboxPaths {
public:
    // 初始化路径（在App启动时调用一次）
    static void initialize();

    // App私有文件目录: /data/data/com.pbox.app/files/
    static std::string appFilesDir();

    // App native库目录: /data/app/.../lib/arm64/ （可执行目录，W^X豁免）
    static std::string nativeLibDir();

    // proot二进制路径（在nativeLibDir下，命名为libpbox_proot.so）
    static std::string prootBin();

    // proot loader路径
    static std::string prootLoader();

    // libtalloc.so路径
    static std::string tallocLib();

    // rootfs存储根目录: files/rootfs/
    static std::string rootfsRoot();

    // 指定容器的rootfs目录
    static std::string rootfsDir(const std::string& tag);

    // 容器配置/启动脚本目录
    static std::string scriptDir();

    // 镜像缓存目录
    static std::string cacheDir();

    // 日志目录
    static std::string logDir();

    // 确保所有必要目录存在
    static void ensureDirs();

private:
    static std::string s_appFilesDir;
    static std::string s_nativeLibDir;
    static bool s_initialized;
};

#endif // PBOX_PATHS_H
