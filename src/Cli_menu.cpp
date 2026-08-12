#include "Cli_menu.h"
#include "image_db.h"
#include "write_log.h"
#include "initialization.h"
#include "call_so.h"
#include <iostream>
#include <vector>
#include <string>

std::shared_ptr<spdlog::logger> get_console_logger();

CliParseResult parse_cli(int argc, char* argv[])
{
    CliParseResult res;
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        size_t eq = arg.find('=');
        // 解析--run_type=debug调试参数
        if (arg.substr(0, 2) == "--")
        {
            std::string key = arg.substr(2, eq - 2);
            std::string val = arg.substr(eq + 1);
            if (key == "run_type")
            {
                res.debug = (val == "debug");
            }
        }
        else if (arg == "list")
        {
            res.cmd = SubCmdType::List;
            if (i + 1 < argc)
                res.list_arg = argv[++i];
            else
                res.list_arg = "";
        }
        else if (arg == "install")
        {
            res.cmd = SubCmdType::Install;
            if (i + 1 < argc)
                res.install_arg = argv[++i];
        }
    }
    return res;
}

int execute_command(const CliParseResult& res, ImageDb& db, const std::string& exe_dir)
{
    auto logger = get_console_logger();

    // list 命令分支
    if (res.cmd == SubCmdType::List)
    {
        const std::string& target_os = res.list_arg;
        // 纯list，输出全部系统名
        if (target_os.empty())
        {
            std::vector<std::string> os_list;
            db.get_all_os(os_list);
            if (os_list.empty())
            {
                DBG(logger, res.debug, "镜像列表为空");
                return 1;
            }
            for (const auto& os_name : os_list)
                std::cout << os_name << '\n';
        }
        // list 系统名，输出对应全部版本
        else
        {
            std::vector<std::string> ver_list = db.get_releases(target_os);
            if (ver_list.empty())
            {
                DBG(logger, res.debug, "{} 不存在任何可用版本", target_os);
                return 1;
            }
            for (const auto& ver : ver_list)
                std::cout << ver << '\n';
        }
        return 0;
    }

    // install 命令分支 install 系统名:版本号
    if (res.cmd == SubCmdType::Install)
    {
        std::string install_param = res.install_arg;
        size_t colon_pos = install_param.find(':');
        if (colon_pos == std::string::npos)
        {
            DBG(logger, res.debug, "参数格式错误，正确格式：install 系统名称:版本");
            return 1;
        }
        std::string os_name = install_param.substr(0, colon_pos);
        std::string release_name = install_param.substr(colon_pos + 1);

        // 调用image_db接口获取架构与镜像路径
        std::vector<std::string> arch_list = db.get_archs(os_name, release_name);
        if (arch_list.empty())
        {
            DBG(logger, res.debug, "{} {} 未匹配到架构", os_name, release_name);
            return 1;
        }
        std::string arch = arch_list.front();
        std::string image_path = db.get_image_path(os_name, release_name, arch, "default");

        if (image_path.empty())
        {
            DBG(logger, res.debug, "未找到镜像 {}:{}", os_name, release_name);
            return 1;
        }
        DBG(logger, res.debug, "加载镜像路径:{}", image_path);
        // 传递路径给动态库启动proot
        return start_proot(image_path, exe_dir, res.debug);
    }

    return -1;
}

// 调用动态库启动proot
int start_proot(const std::string& rootfs_path, const std::string& exe_dir, bool debug)
{
    auto logger = get_console_logger();
    std::string init_so = exe_dir + "/lib/libinitialization.so";
    void* handle = nullptr;

    using RunProotFunc = int (*)(const char*, const char*);
    auto run_proot = load_dynamic_lib<RunProotFunc>(init_so, "run_proot", &handle);

    if (!handle)
    {
        DBG(logger, debug, "加载libinitialization.so失败:{}", dlerror());
        return -1;
    }
    if (!run_proot)
    {
        DBG(logger, debug, "获取run_proot导出函数失败:{}", dlerror());
        close_lib_handle(handle);
        return -2;
    }

    int ret = run_proot(exe_dir.c_str(), rootfs_path.c_str());
    if (ret == -1)
    {
        DBG(logger, debug, "libinitialization内部错误：dlopen加载libproot.so失败");
    }
    else if (ret == -2)
    {
        DBG(logger, debug, "libinitialization内部错误：未找到proot main符号");
    }

    close_lib_handle(handle);
    return ret;
}
