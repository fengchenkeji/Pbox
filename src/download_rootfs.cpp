#include "download_rootfs.hpp"
#include "write_log.h"
#include "call_so.h"

#include <spdlog/spdlog.h>
#include <iostream>
#include <filesystem>
#include <cstdlib>
#include <vector>
#include <cstdio>
#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/wait.h>

namespace fs = std::filesystem;

std::vector<MirrorSource> get_mirror_list()
{
    return {
        {"清华镜像源", "https://mirrors.tuna.tsinghua.edu.cn/lxc-images"},
        {"南阳理工镜像源", "https://mirror.nyist.edu.cn/lxc-images"},
        {"网易镜像源", "https://mirrors.163.com/lxc-images"},
        {"阿里云镜像源", "https://mirrors.aliyun.com/lxc-images"},
        {"LXC官方源", "https://images.linuxcontainers.org"}
    };
}

bool download_rootfs(const std::string& relative_path,
                    const std::string& save_path,
                    bool debug)
{
    auto logger = get_console_logger();
    auto mirrors = get_mirror_list();
    for (const auto& mirror : mirrors)
    {
        std::string url = mirror.base_url;
        if (!relative_path.empty() && relative_path.front() != '/')
            url += "/";
        url += relative_path;
        DBG(logger, debug, "尝试从【{}】下载: {}", mirror.name, url);
        std::string cmd = "wget -q --timeout=30 --tries=2 -O " + save_path + " " + url;
        int ret = system(cmd.c_str());
        if (ret == 0 && fs::exists(save_path) && fs::file_size(save_path) > 0)
        {
            DBG(logger, debug, "【{}】下载成功", mirror.name);
            return true;
        }
        DBG(logger, debug, "【{}】下载失败，切换下一镜像源", mirror.name);
        if (fs::exists(save_path))
            fs::remove(save_path);
    }
    std::cerr << "[错误] 所有镜像源下载rootfs压缩包失败，请检查网络或稍后重试\n";
    DBG(logger, debug, "全部镜像源下载rootfs失败");
    return false;
}

static std::string expand_tilde(const std::string& s)
{
    if (s.empty() || s[0] != '~')
        return s;

    const char* home = getenv("HOME");
    if (!home)
    {
        struct passwd* pw = getpwuid(getuid());
        if (pw != nullptr)
        {
            home = pw->pw_dir;
        }
    }
    if (!home)
        return s;

    if (s.size() == 1)
    {
        return std::string(home);
    }
    if (s[1] == '/')
    {
        return std::string(home) + s.substr(1);
    }
    return s;
}

using ProotMainFunc = int (*)(int argc, char** argv);

bool extract_rootfs(const std::string& tar_path,
                    const std::string& rootfs_dir,
                    const std::string& arch,
                    bool debug)
{
    auto logger = get_console_logger();

    if (!fs::exists(tar_path))
    {
        std::cerr << "[错误] 压缩包不存在，解压失败\n";
        DBG(logger, debug, "压缩包不存在: {}", tar_path);
        return false;
    }

    try
    {
        fs::create_directories(rootfs_dir);
    }
    catch (const fs::filesystem_error& e)
    {
        std::cerr << "[错误] 创建rootfs根目录权限失败: " << e.what() << "\n";
        DBG(logger, debug, "创建rootfs目录异常:{}", e.what());
        return false;
    }

    void* so_handle = nullptr;
    const std::string libproot_path = "./lib/libproot.so";
    auto proot_main = load_dynamic_lib<ProotMainFunc>(libproot_path, "main", &so_handle);
    if (!proot_main || so_handle == nullptr)
    {
        DBG(logger, debug, "加载libproot.so失败 dlerror={}", dlerror());
        close_lib_handle(so_handle);
        return false;
    }

    std::string real_tar = expand_tilde(tar_path);
    std::string real_dest = expand_tilde(rootfs_dir);

    DBG(logger, debug, "【静默模式】执行 proot --link2symlink tar -pJxf {} -C {}", real_tar, real_dest);

    std::vector<const char*> argv_list = {
        "proot",
        "--link2symlink",
        "tar",
        "-pJxf",
        real_tar.c_str(),
        "-C",
        real_dest.c_str(),
        nullptr
    };
    int argc = static_cast<int>(argv_list.size()) - 1;
    char** argv = const_cast<char**>(argv_list.data());

    int ret_code = -1;
    pid_t pid = fork();
    if (pid == 0)
    {
        // =====子进程执行proot_main=====
        int fd_stdout = dup(STDOUT_FILENO);
        int fd_stderr = dup(STDERR_FILENO);
        int devnull = open("/dev/null", O_RDWR);

        dup2(devnull, STDOUT_FILENO);
        dup2(devnull, STDERR_FILENO);

        void* child_handle = nullptr;
        auto child_proot_main = load_dynamic_lib<ProotMainFunc>(libproot_path, "proot_main", &child_handle);
        if(child_proot_main && child_handle)
        {
            child_proot_main(argc, argv);
        }
        close_lib_handle(child_handle);

        dup2(fd_stdout, STDOUT_FILENO);
        dup2(fd_stderr, STDERR_FILENO);
        close(devnull);
        close(fd_stdout);
        close(fd_stderr);
        _exit(127);
    }
    else if (pid > 0)
    {
        // =====父进程等待子进程结束=====
        int wstatus;
        waitpid(pid, &wstatus, 0);
        if(WIFEXITED(wstatus))
        {
            ret_code = WEXITSTATUS(wstatus);
        }
        DBG(logger, debug, "proot‑tar子进程退出码={}", ret_code);
    }
    else
    {
        DBG(logger, debug, "fork子进程失败");
        close_lib_handle(so_handle);
        return false;
    }

    close_lib_handle(so_handle);

    if (ret_code != 0)
    {
        DBG(logger, debug, "proot‑tar 解压失败 ret={}", ret_code);
        try { fs::remove_all(real_dest); } catch (...) {}
        return false;
    }

    bool has_valid_file = false;
    try
    {
        for (auto& entry : fs::recursive_directory_iterator(real_dest))
        {
            if (entry.is_regular_file())
            {
                has_valid_file = true;
                break;
            }
        }
    }
    catch (...) {}

    if (!has_valid_file)
    {
        std::cerr << "[错误] 解压后无有效系统文件\n";
        DBG(logger, debug, "解压后无有效文件");
        try { fs::remove_all(real_dest); } catch (...) {}
        return false;
    }

    std::string resolv_file = fs::path(real_dest) / "etc" / "resolv.conf";
    fs::remove(resolv_file);
    FILE* fp = fopen(resolv_file.c_str(), "w");
    if (fp != nullptr)
    {
        fputs("nameserver 114.114.114.114\n", fp);
        fputs("nameserver 223.5.5.5\n", fp);
        fputs("nameserver 8.8.8.8\n", fp);
        fclose(fp);
        DBG(logger, debug, "已修复 resolv.conf DNS配置");
    }

    DBG(logger, debug, "proot‑tar【静默解压】完成，目录:{}", real_dest);
    return true;
}
