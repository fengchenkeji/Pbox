#include "Cli_menu.h"
#include "image_db.h"
#include "write_log.h"
#include "initialization.h"
#include "call_so.h"
#include "download_rootfs.hpp"
#include "container_config.h"

#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <fstream>
#include <sys/utsname.h>

namespace fs = std::filesystem;
std::shared_ptr<spdlog::logger> get_console_logger();

void print_help()
{
    std::cout << "Pbox Proot 容器工具（LXC镜像版）\n";
    std::cout << "用法:\n";
    std::cout << "  pbox --run_type=debug install ubuntu:jammy   下载并安装容器\n";
    std::cout << "  pbox list                                    列出所有系统\n";
    std::cout << "  pbox list ubuntu                             列出ubuntu所有版本\n";
    std::cout << "  pbox login ubuntu:jammy                      进入已安装容器\n";
    std::cout << "  pbox -h / --help                             显示本帮助\n";
}

CliParseResult parse_cli(int argc, char* argv[])
{
    CliParseResult res{};
    if (argc <= 1)
    {
        res.cmd = SubCmdType::Help;
        return res;
    }
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        size_t eq = arg.find('=');
        if (arg == "-h" || arg == "--help")
        {
            res.cmd = SubCmdType::Help;
            return res;
        }
        if (arg.substr(0, 2) == "--")
        {
            std::string key = arg.substr(2, eq - 2);
            std::string val = arg.substr(eq + 1);
            if (key == "run_type")
                res.debug = (val == "debug");
        }
        else if (arg == "list")
        {
            res.cmd = SubCmdType::List;
            res.list_arg = (i + 1 < argc) ? argv[++i] : "";
        }
        else if (arg == "install")
        {
            res.cmd = SubCmdType::Install;
            if (i + 1 < argc) res.install_arg = argv[++i];
        }
        else if (arg == "login")
        {
            res.cmd = SubCmdType::Login;
            if (i + 1 < argc) res.install_arg = argv[++i];
        }
    }
    return res;
}

static std::string get_native_arch()
{
    struct utsname u;
    if (uname(&u) != 0) return "";
    std::string machine = u.machine;
    if (machine == "armv8l" || machine == "armv7l" || machine == "armhf")
        return "armhf";
    if (machine == "aarch64" || machine == "arm64")
        return "arm64";
    if (machine == "x86_64" || machine == "amd64")
        return "amd64";
    if (machine == "riscv64")
        return "riscv64";
    return machine;
}

static std::string match_native_arch(const std::vector<std::string>& arch_list)
{
    std::string native = get_native_arch();
    if (native.empty()) return "";
    for (const auto& a : arch_list)
        if (a == native) return a;
    return "";
}

struct ContainerStatus
{
    bool installed = false;
    bool mark_exists = false;
    bool rootfs_exists = false;
    std::string script_file;
    std::string rootfs_dir;
    std::string tag;
    std::string os_name;
    std::string release_name;
};

static bool parse_and_check_container(const std::string& install_param,
                                       const std::string& exe_dir,
                                       ContainerStatus& status,
                                       bool debug)
{
    auto logger = get_console_logger();
    size_t colon_pos = install_param.find(':');
    if (colon_pos == std::string::npos)
    {
        std::cerr << "[错误] 参数格式错误，正确格式：install/login 系统名称:版本\n";
        return false;
    }
    status.os_name = install_param.substr(0, colon_pos);
    status.release_name = install_param.substr(colon_pos + 1);
    status.tag = status.os_name + "_" + status.release_name;
    status.script_file = exe_dir + "/proot/start_script/" + status.tag;
    status.rootfs_dir = exe_dir + "/proot/container/" + status.tag;

    try
    {
        status.mark_exists = false;
        if (fs::exists(status.script_file) && fs::is_regular_file(status.script_file))
        {
            if (fs::file_size(status.script_file) > 0)
                status.mark_exists = true;
        }

        status.rootfs_exists = false;
        if (fs::exists(status.rootfs_dir) && fs::is_directory(status.rootfs_dir))
            status.rootfs_exists = true;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[错误] 检查安装状态异常\n";
        DBG(logger, debug, "检查安装状态异常: {}", e.what());
        return false;
    }
    status.installed = status.mark_exists && status.rootfs_exists;
    return true;
}

int execute_command(const CliParseResult& res, ImageDb& db, const std::string& exe_dir)
{
    auto logger = get_console_logger();

    if (res.cmd == SubCmdType::Help)
    {
        print_help();
        return 0;
    }

    if (res.cmd == SubCmdType::List)
    {
        const std::string& target_os = res.list_arg;
        if (target_os.empty())
        {
            std::vector<std::string> os_list;
            db.get_all_os(os_list);
            if (os_list.empty())
            {
                std::cerr << "[提示] 镜像列表为空\n";
                return 1;
            }
            for (const auto& os_name : os_list)
                std::cout << os_name << '\n';
        }
        else
        {
            std::vector<std::string> ver_list = db.get_releases(target_os);
            if (ver_list.empty())
            {
                std::cerr << "[提示] " << target_os << " 不存在任何可用版本\n";
                return 1;
            }
            for (const auto& ver : ver_list)
                std::cout << ver << '\n';
        }
        return 0;
    }

    ContainerStatus status;
    if (!parse_and_check_container(res.install_arg, exe_dir, status, res.debug))
        return 1;

    if (res.cmd == SubCmdType::Login)
    {
        if (!status.installed)
        {
            std::cerr << "[错误] " << status.tag << " 未安装，请先执行: pbox install "
                      << status.os_name << ":" << status.release_name << "\n";
            return 1;
        }
        std::cout << "[信息] 正在启动容器 " << status.tag << " ...\n";
        // 修改点1
        return start_proot(status.rootfs_dir, exe_dir, logger, res.debug);
    }

    // Install 命令
    if (status.installed)
    {
        std::cout << "[信息] " << status.tag << " 已安装，直接启动容器\n";
        // 修改点2
        return start_proot(status.rootfs_dir, exe_dir, logger, res.debug);
    }

    // 清理无标记的残留rootfs
    if (fs::exists(status.rootfs_dir) && !status.mark_exists)
    {
        DBG(logger, res.debug, "检测到残留rootfs目录，清理:{}", status.rootfs_dir);
        try { fs::remove_all(status.rootfs_dir); }
        catch (const std::exception& e) {
            std::cerr << "[警告] 清理残留失败: " << e.what() << "\n";
        }
    }

    DBG(logger, res.debug, "开始安装容器: {}", status.tag);

    std::vector<std::string> arch_list = db.get_archs(status.os_name, status.release_name);
    if (arch_list.empty())
    {
        std::cerr << "[错误] " << status.os_name << " " << status.release_name
                  << " 未匹配到架构，安装失败\n";
        return 1;
    }
    std::string arch = match_native_arch(arch_list);
    if (arch.empty())
    {
        std::cerr << "[错误] 未找到与本机架构匹配的镜像\n";
        return 1;
    }
    DBG(logger, res.debug, "自动匹配本机架构: {}", arch);

    std::string relative_path = db.get_image_path(status.os_name, status.release_name, arch, "default");
    if (relative_path.empty())
    {
        std::cerr << "[错误] 未找到镜像，安装失败\n";
        return 1;
    }

    try
    {
        fs::create_directories(fs::path(status.script_file).parent_path());
    }
    catch (const std::exception& e)
    {
        std::cerr << "[错误] 创建目录失败\n";
        return -1;
    }

    std::string tar_path = exe_dir + "/" + status.tag + "_rootfs.tar.xz";
    if (!download_rootfs(relative_path, tar_path, res.debug))
    {
        std::cerr << "[错误] rootfs下载失败，安装终止\n";
        return -1;
    }

    if (!extract_rootfs(tar_path, status.rootfs_dir, arch, res.debug))
    {
        std::cerr << "[错误] rootfs解压失败，安装终止\n";
        return -1;
    }

    // 生成默认配置文件（同时作为安装标记）
    if (!generate_default_config(status.script_file, status.os_name, status.release_name, arch, logger, res.debug))
    {
        std::cerr << "[错误] 写入安装标记失败\n";
        return -1;
    }

    try { if (fs::exists(tar_path)) fs::remove(tar_path); } catch (...) {}

    DBG(logger, res.debug, "{} 安装完成，启动容器", status.tag);
    std::cout << "[成功] " << status.tag << " 安装完成\n";
    // 修改点3
    return start_proot(status.rootfs_dir, exe_dir, logger, res.debug);
}

int start_proot(const std::string& rootfs_path, const std::string& exe_dir, LoggerPtr logger, bool debug)
{
    std::string init_so = exe_dir + "/lib/libinitialization.so";
    void* handle = nullptr;
    using RunProotFunc = int (*)(const char*, const char*, LoggerPtr, bool);
    auto run_proot = load_dynamic_lib<RunProotFunc>(init_so, "run_proot", &handle);

    if (!handle)
    {
        std::cerr << "[错误] 加载libinitialization.so失败，启动容器失败\n";
        DBG(logger, debug, "加载libinitialization.so失败:{}", dlerror());
        return -1;
    }
    if (!run_proot)
    {
        std::cerr << "[错误] 获取run_proot函数失败\n";
        DBG(logger, debug, "获取run_proot导出函数失败:{}", dlerror());
        close_lib_handle(handle);
        return -2;
    }

    int ret = run_proot(exe_dir.c_str(), rootfs_path.c_str(), logger, debug);
    if (ret == -1)
        std::cerr << "[错误] libproot.so加载失败\n";
    else if (ret == -2)
        std::cerr << "[错误] 未找到proot main符号\n";

    close_lib_handle(handle);
    return ret;
}
