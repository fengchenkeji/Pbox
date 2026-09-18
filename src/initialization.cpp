/*
 * Pbox - Termux proot container manager
 * Copyright (C) 2026  fengchenkeji
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "initialization.h"
#include "container_config.h"
#include "call_so.h"

#include <dlfcn.h>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

// 检查 rootfs 内指定路径的文件是否存在且可执行
static bool shell_exists(const char* rootfs_path, const char* shell_rel_path)
{
    std::string full = std::string(rootfs_path) + shell_rel_path;
    struct stat st;
    if (stat(full.c_str(), &st) != 0)
        return false;
    return S_ISREG(st.st_mode) && (st.st_mode & S_IXUSR);
}

// 启动 socat 端口转发后台进程
static std::vector<pid_t> start_port_forwarding(const ContainerConfig& cfg, LoggerPtr logger, bool debug)
{
    std::vector<pid_t> pids;
    if (cfg.portForwards.empty()) return pids;

    if (system("command -v socat >/dev/null 2>&1") != 0)
    {
        DBG(logger, debug, "socat 未安装，跳过端口转发（pkg install socat）");
        return pids;
    }

    for (const auto& pf : cfg.portForwards)
    {
        pid_t pid = fork();
        if (pid == 0)
        {
            prctl(PR_SET_PDEATHSIG, SIGKILL);
            std::string listen_arg = "TCP-LISTEN:" + std::to_string(pf.hostPort) + ",fork,reuseaddr";
            std::string target_arg = "TCP:127.0.0.1:" + std::to_string(pf.containerPort);

            int devnull = open("/dev/null", O_RDWR);
            if (devnull >= 0)
            {
                dup2(devnull, STDOUT_FILENO);
                dup2(devnull, STDERR_FILENO);
                close(devnull);
            }
            execlp("socat", "socat", listen_arg.c_str(), target_arg.c_str(), (char*)nullptr);
            _exit(127);
        }
        else if (pid > 0)
        {
            pids.push_back(pid);
            DBG(logger, debug, "端口转发已启动: 宿主{} -> 容器{} (pid={})",
                pf.hostPort, pf.containerPort, pid);
        }
    }
    return pids;
}

static void stop_port_forwarding(const std::vector<pid_t>& pids)
{
    for (pid_t pid : pids) kill(pid, SIGTERM);
    for (pid_t pid : pids) { int s; waitpid(pid, &s, WNOHANG); }
}

// 查找 proot 可执行文件路径
// proot 二进制随 Pbox 包一起安装，位于 ${exe_dir}/bin/proot
// 使用相对路径（相对于 pbox 安装目录），不依赖系统 proot
static std::string find_proot_binary(const std::string& exe_dir, LoggerPtr logger, bool debug)
{
    std::string bundled = exe_dir + "/bin/proot";
    if (access(bundled.c_str(), X_OK) == 0)
    {
        DBG(logger, debug, "使用自带 proot: {}", bundled);
        return bundled;
    }

    DBG(logger, debug, "未找到自带 proot: {}", bundled);
    return "";
}

extern "C" int run_proot(const char* exe_dir,
                         const char* rootfs_path,
                         LoggerPtr logger,
                         bool debug)
{
    std::string exeDir(exe_dir);
    std::string rootfsDir(rootfs_path);

    // 加载容器配置
    ContainerConfig cfg;
    load_container_config(exeDir, rootfsDir, cfg, logger, debug);

    // 查找 proot 可执行文件
    std::string proot_bin = find_proot_binary(exeDir, logger, debug);
    if (proot_bin.empty())
    {
        DBG(logger, debug, "未找到 proot 可执行文件，请安装 proot 包");
        return -1;
    }

    // 设置 proot loader 环境变量（Android 版 proot 使用外部 loader）
    // 自带 loader 文件位于 lib/ 目录
    std::string loader_path = exeDir + "/lib/libproot-loader.so";
    if (access(loader_path.c_str(), F_OK) == 0)
    {
        setenv("PROOT_LOADER", loader_path.c_str(), 1);
        DBG(logger, debug, "设置 PROOT_LOADER={}", loader_path);
    }
    // 64 位架构下还需要 32 位兼容 loader
    std::string loader32_path = exeDir + "/lib/libproot-loader32.so";
    if (access(loader32_path.c_str(), F_OK) == 0)
    {
        setenv("PROOT_LOADER_32", loader32_path.c_str(), 1);
        DBG(logger, debug, "设置 PROOT_LOADER_32={}", loader32_path);
    }

    // 选择 shell
    const char* shell_cmd;
    const char* shell_arg = nullptr;
    if (shell_exists(rootfs_path, "/usr/bin/bash"))
    {
        shell_cmd = "/bin/bash";
        shell_arg = "--login";
    }
    else
    {
        shell_cmd = "/bin/sh";
    }

    // TERM
    const char* term_env = std::getenv("TERM");
    if (!term_env || strlen(term_env) == 0)
        term_env = "xterm-256color";

    // /dev/shm 绑定源
    std::string shm_source = rootfsDir + "/root";

    // ===== 构建 proot 启动参数 =====
    std::vector<std::string> args_vec;
    args_vec.push_back("proot");
    args_vec.push_back("--link2symlink");
    args_vec.push_back("--kill-on-exit");
    args_vec.push_back("-0");
    args_vec.push_back("-r");
    args_vec.push_back(rootfs_path);

    // 系统必备绑定
    args_vec.push_back("-b"); args_vec.push_back("/dev");
    args_vec.push_back("-b"); args_vec.push_back("/proc");
    args_vec.push_back("-b"); args_vec.push_back(shm_source + ":/dev/shm");

    // 用户配置绑定挂载
    for (const auto& mnt : cfg.mounts)
    {
        args_vec.push_back("-b");
        args_vec.push_back(mnt);
        DBG(logger, debug, "添加绑定挂载: {}", mnt);
    }

    // 工作目录
    args_vec.push_back("-w");
    args_vec.push_back("/root");

    // 干净环境
    args_vec.push_back("/usr/bin/env");
    args_vec.push_back("-i");
    args_vec.push_back("HOME=/root");
    args_vec.push_back("PATH=/usr/local/sbin:/usr/local/bin:/bin:/usr/bin:/sbin:/usr/sbin:/usr/games:/usr/local/games");
    args_vec.push_back(std::string("TERM=") + term_env);
    args_vec.push_back("LANG=C.UTF-8");

    // 启动 shell
    args_vec.push_back(shell_cmd);
    if (shell_arg) args_vec.push_back(shell_arg);

    // 转 char* 数组
    std::vector<char*> argv;
    for (auto& s : args_vec)
        argv.push_back(const_cast<char*>(s.c_str()));
    int argc = static_cast<int>(argv.size());

    DBG(logger, debug, "proot 启动参数 ({}个):", argc);
    for (int i = 0; i < argc; ++i)
        DBG(logger, debug, "  argv[{}] = {}", i, argv[i]);

    // 启动端口转发
    std::vector<pid_t> port_pids = start_port_forwarding(cfg, logger, debug);

    // ===== fork + exec 启动 proot（完全进程隔离） =====
    pid_t pid = fork();
    if (pid < 0)
    {
        DBG(logger, debug, "fork 失败: {}", strerror(errno));
        stop_port_forwarding(port_pids);
        return -2;
    }

    if (pid == 0)
    {
        // 子进程：直接执行 proot 可执行文件
        // 不做任何清理，execve 后完全替换为 proot 进程
        execv(proot_bin.c_str(), argv.data());
        // execve 失败才会到这里
        fprintf(stderr, "[Pbox] execv proot 失败: %s\n", strerror(errno));
        _exit(127);
    }

    // 父进程：等待 proot 子进程退出
    int status = 0;
    waitpid(pid, &status, 0);

    // 清理端口转发
    stop_port_forwarding(port_pids);

    if (WIFEXITED(status))
    {
        return WEXITSTATUS(status);
    }
    return -1;
}

// 静态存储最近获取的版本号（避免返回栈指针）
static std::string g_proot_version_cache;

extern "C" const char* get_proot_version(const char* exe_dir, bool debug)
{
    std::string exeDir(exe_dir ? exe_dir : ".");

    // 查找 proot 可执行文件
    LoggerPtr null_logger = nullptr;
    std::string proot_bin = find_proot_binary(exeDir, null_logger, debug);
    if (proot_bin.empty())
    {
        g_proot_version_cache = "";
        return g_proot_version_cache.c_str();
    }

    // 执行 proot --version，捕获输出
    std::string cmd = proot_bin + " --version 2>/dev/null";
    FILE* fp = popen(cmd.c_str(), "r");
    if (!fp)
    {
        g_proot_version_cache = "";
        return g_proot_version_cache.c_str();
    }

    char buf[256] = {0};
    std::string output;
    while (fgets(buf, sizeof(buf), fp))
    {
        output += buf;
    }
    pclose(fp);

    // 解析版本号: 期望格式 "proot 5.4.0" 或 "proot version 5.4.0"
    g_proot_version_cache = "";
    size_t pos = output.find("version");
    if (pos != std::string::npos)
    {
        pos += 7; // skip "version"
        while (pos < output.size() && (output[pos] == ' ' || output[pos] == '\t'))
            pos++;
        size_t end = pos;
        while (end < output.size() && output[end] != '\n' && output[end] != '\r' && output[end] != ' ')
            end++;
        g_proot_version_cache = output.substr(pos, end - pos);
    }
    else
    {
        // 尝试直接找版本号格式 x.y.z
        size_t start = output.find_first_of("0123456789");
        if (start != std::string::npos)
        {
            size_t end = start;
            while (end < output.size() &&
                   (isdigit(output[end]) || output[end] == '.'))
                end++;
            g_proot_version_cache = output.substr(start, end - start);
        }
    }

    return g_proot_version_cache.c_str();
}
