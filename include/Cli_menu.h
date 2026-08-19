#pragma once
#include <string>
#include "write_log.h"

enum class SubCmdType
{
    Help,
    List,
    Install,
    Login
};

struct CliParseResult
{
    SubCmdType cmd = SubCmdType::Help;
    bool debug = false;
    std::string list_arg;
    std::string install_arg;
};

CliParseResult parse_cli(int argc, char* argv[]);
int execute_command(const CliParseResult& res, class ImageDb& db, const std::string& exe_dir);
int start_proot(const std::string& rootfs_path, const std::string& exe_dir, LoggerPtr logger, bool debug);
