#pragma once
#include <vector>
#include <string>

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
// 仅调用so接口，内部无dlopen
int start_proot(const std::string& rootfs_path, const std::string& exe_dir, bool debug);
