#include "initialization.h"
#include <dlfcn.h>
#include <string>
#include <vector>
#include <cstdio>
#include <sys/stat.h>
#include <unistd.h>

using ProotMain = int (*)(int argc, char** argv);

// 检查 rootfs 内指定路径的文件是否存在且可执行
static bool shell_exists(const char* rootfs_path, const char* shell_rel_path)
{
    std::string full = std::string(rootfs_path) + shell_rel_path;
    struct stat st;
    if (stat(full.c_str(), &st) != 0)
        return false;
    return S_ISREG(st.st_mode) && (st.st_mode & S_IXUSR);
}

extern "C" int run_proot(const char* exe_dir, const char* rootfs_path)
{
    std::string proot_so_path = std::string(exe_dir) + "/lib/libproot.so";

    void* handle = dlopen(proot_so_path.c_str(), RTLD_NOW | RTLD_GLOBAL);
    if (!handle)
    {
        return -1;
    }

    auto proot_main = reinterpret_cast<ProotMain>(dlsym(handle, "main"));
    if (!proot_main)
    {
        dlclose(handle);
        return -2;
    }

    // 选择启动 shell：优先 bash --login，fallback /bin/sh
    const char* shell_cmd;
    const char* shell_arg = nullptr;
    if (shell_exists(rootfs_path, "/usr/bin/bash"))
    {
        shell_cmd = "/usr/bin/bash";
        shell_arg = "--login";
    }
    else
    {
        shell_cmd = "/bin/sh";
    }

    // 构建 proot 启动参数
    // --link2symlink: Android 不支持硬链接，转为符号链接
    // --kill-on-exit: proot 退出时杀掉所有子进程
    // --sysvipc: 允许 System V IPC（部分程序需要）
    // -0: 模拟 root 用户
    // -r: 指定 rootfs 根目录
    // -w: 工作目录
    // -b: 绑定挂载系统目录
    std::vector<std::string> args_vec = {
        "proot",
        "--link2symlink",
        "--kill-on-exit",
        "--sysvipc",
        "-0",
        "-r", rootfs_path,
        "-w", "/root",
        "-b", "/dev",
        "-b", "/proc",
        "-b", "/sys",
        shell_cmd
    };
    if (shell_arg != nullptr)
    {
        args_vec.push_back(shell_arg);
    }

    // 转换为 char* 数组
    std::vector<char*> argv;
    for (auto& s : args_vec)
    {
        argv.push_back(const_cast<char*>(s.c_str()));
    }
    int argc = static_cast<int>(argv.size());

    int ret_code = proot_main(argc, argv.data());

    dlclose(handle);
    return ret_code;
}
