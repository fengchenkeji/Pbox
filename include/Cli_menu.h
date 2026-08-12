#pragma once
#include <vector>
#include <string>
class ImageDb;

enum class SubCmdType
{
    None,
    List,
    Install,
    Login
};

struct CliParseResult
{
    SubCmdType cmd = SubCmdType::None;
    std::string list_arg;
    std::string install_arg;
    std::string login_arg;
    bool debug = false;
};

CliParseResult parse_cli(int argc, char* argv[]);
int execute_command(const CliParseResult& res, ImageDb& db, const std::string& exe_dir);
// 仅调用so接口，内部无dlopen
int start_proot(const std::string& rootfs_path, const std::string& exe_dir, bool debug);
