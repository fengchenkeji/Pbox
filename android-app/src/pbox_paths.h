// pbox_paths.h - Pbox Android 路径管理
#ifndef PBOX_PATHS_H
#define PBOX_PATHS_H

#include <string>
#include <filesystem>

namespace fs = std::filesystem;

class PboxPaths {
public:
    static void initialize();
    static void ensureDirs();

    static std::string appFilesDir();
    static std::string nativeLibDir();

    static std::string prootBin();
    static std::string prootLoader();
    static std::string tallocLib();

    static std::string rootfsRoot();
    static std::string rootfsDir(const std::string& tag);
    static std::string cacheDir();
    static std::string logDir();
    static std::string scriptsDir();

private:
    static std::string s_appFilesDir;
    static std::string s_nativeLibDir;
    static bool s_initialized;
};

#endif // PBOX_PATHS_H
