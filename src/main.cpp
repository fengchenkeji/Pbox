#include "Cli_menu.h"
#include "image_db.h"
#include "write_log.h"
#include <string>

std::shared_ptr<spdlog::logger> get_console_logger();

int main(int argc, char* argv[])
{
    // 初始化日志
    init_error_file_log("./logs");
    auto logger = get_console_logger();

    // 获取程序运行目录
    std::string full_path = argv[0];
    size_t slash = full_path.rfind('/');
    std::string exe_dir;
    if (slash != std::string::npos)
        exe_dir = full_path.substr(0, slash);
    else
        exe_dir = ".";

    // 解析命令行参数
    CliParseResult cli_arg = parse_cli(argc, argv);
    ImageDb db;
    std::string img_file = exe_dir + "/res/images.txt";
    db.load(img_file, cli_arg.debug);

    return execute_command(cli_arg, db, exe_dir);
}
