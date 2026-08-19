#pragma once
#include <vector>
#include <string>
#include "write_log.h"

class ImageDb;

enum class SubCmdType
{
    None,
    Help,
    List,
    Install,
    Login
};

struct CliParseResult
{
    SubCmdType cmd = SubCmdType::None;
    bool debug = false;
    std::string list_arg;
    std::string install_arg;
};

CliParseResult parse_cli(int argc, char* argv[]);
int execute_command(const CliParseResult& res, ImageDb& db, const std::string& exe_dir);

// 启动 proot 容器：内部加载 libinitialization.so 并调用 run_proot
int start_proot(const std::string& rootfs_path,
                const std::string& exe_dir,
                LoggerPtr logger,
                bool debug);
