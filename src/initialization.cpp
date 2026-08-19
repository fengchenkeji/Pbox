#include "initialization.h"
#include "write_log.h"

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

std::shared_ptr<spdlog::logger> get_console_logger();

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

// 启动 socat 端口转发后台进程
// proot 共享宿主机网络，容器端口即宿主机端口；
// 通过 socat 将宿主机 host_port 转发到 localhost:container_port
static std::vector<pid_t> start_port_forwarding(const ContainerConfig* config, bool debug)
{
    auto logger = get_console_logger();
    std::vector<pid_t> pids;

    if (!config || config->ports.empty())
        return pids;

    // 检查 socat 是否可用
    if (system("command -v socat >/dev/null 2>&1") != 0)
    {
        DBG(logger, debug, "socat 未安装，跳过端口转发（请执行: pkg install socat）");
        return pids;
    }

    for (const auto& rule : config->ports)
    {
        pid_t pid = fork();
        if (pid == 0)
        {
            // 子进程：父进程退出时自动收到 SIGKILL，避免 socat 残留
            prctl(PR_SET_PDEATHSIG, SIGKILL);

            // 执行 socat
            std::string listen_arg = "TCP-LISTEN:" + std::to_string(rule.host_port) + ",fork,reuseaddr";
            std::string target_arg = "TCP:127.0.0.1:" + std::to_string(rule.container_port);

            // 重定向输出到 /dev/null
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
                rule.host_port, rule.container_port, pid);
        }
    }
    return pids;
}

// 清理 socat 后台进程
static void stop_port_forwarding(const std::vector<pid_t>& pids)
{
    for (pid_t pid : pids)
    {
        kill(pid, SIGTERM);
    }
    for (pid_t pid : pids)
    {
        int status;
        waitpid(pid, &status, WNOHANG);
    }
}

extern "C" int run_proot(const char* exe_dir, const char* rootfs_path, const ContainerConfig* config)
{
    auto logger = get_console_logger();
    bool debug = true; // initialization 内部始终打印关键日志

    std::string proot_so_path = std::string(exe_dir) + "/lib/libproot.so";

    void* handle = dlopen(proot_so_path.c_str(), RTLD_NOW | RTLD_GLOBAL);
    if (!handle)
    {
        DBG(logger, debug, "dlopen libproot.so 失败: {}", dlerror());
        return -1;
    }

    auto proot_main = reinterpret_cast<ProotMain>(dlsym(handle, "main"));
    if (!proot_main)
    {
        DBG(logger, debug, "dlsym main 失败: {}", dlerror());
        dlclose(handle);
        return -2;
    }

    // 选择启动 shell：优先 bash --login，fallback /bin/sh
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

    // 获取 TERM 环境变量
    const char* term_env = std::getenv("TERM");
    if (!term_env || strlen(term_env) == 0)
        term_env = "xterm-256color";

    // /dev/shm 绑定源：使用 rootfs 内的 /root 目录（和 bash 脚本逻辑一致）
    std::string shm_source = std::string(rootfs_path) + "/root";

    // ===== 构建 proot 启动参数（完全对应 bash 启动脚本）=====
    std::vector<std::string> args_vec;

    // proot 基础参数
    args_vec.push_back("proot");
    args_vec.push_back("--link2symlink");
    args_vec.push_back("--kill-on-exit");
    args_vec.push_back("-0");
    args_vec.push_back("-r");
    args_vec.push_back(rootfs_path);

    // 绑定挂载：系统必备
    args_vec.push_back("-b");
    args_vec.push_back("/dev");
    args_vec.push_back("-b");
    args_vec.push_back("/proc");
    args_vec.push_back("-b");
    args_vec.push_back(shm_source + ":/dev/shm");

    // 绑定挂载：用户配置（从 ContainerConfig 读取）
    if (config)
    {
        for (const auto& bind : config->binds)
        {
            // 检查宿主机路径是否存在
            struct stat st;
            if (stat(bind.host_path.c_str(), &st) != 0)
            {
                DBG(logger, debug, "绑定路径不存在，跳过: {}", bind.host_path);
                continue;
            }
            args_vec.push_back("-b");
            args_vec.push_back(bind.host_path + ":" + bind.container_path);
            DBG(logger, debug, "添加绑定挂载: {}:{}", bind.host_path, bind.container_path);
        }
    }

    // 工作目录
    args_vec.push_back("-w");
    args_vec.push_back("/root");

    // 使用 /usr/bin/env -i 构建干净环境（对应 bash 脚本中的 /usr/bin/env -i）
    args_vec.push_back("/usr/bin/env");
    args_vec.push_back("-i");
    args_vec.push_back("HOME=/root");
    args_vec.push_back("PATH=/usr/local/sbin:/usr/local/bin:/bin:/usr/bin:/sbin:/usr/sbin:/usr/games:/usr/local/games");
    args_vec.push_back(std::string("TERM=") + term_env);
    args_vec.push_back("LANG=C.UTF-8");

    // 启动 shell
    args_vec.push_back(shell_cmd);
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

    DBG(logger, debug, "proot 启动参数 ({}个):", argc);
    for (int i = 0; i < argc; ++i)
    {
        DBG(logger, debug, "  argv[{}] = {}", i, argv[i]);
    }

    // 启动端口转发（socat 后台进程）
    std::vector<pid_t> port_pids = start_port_forwarding(config, debug);

    // 调用 proot main（此调用不会返回，proot 内部直接 exit）
    int ret_code = proot_main(argc, argv.data());

    // 如果 proot_main 意外返回，清理端口转发
    stop_port_forwarding(port_pids);

    dlclose(handle);
    return ret_code;
}
