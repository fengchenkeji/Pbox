//main.cpp

#include <iostream>
#include <string>
#include <filesystem>
#include "initialization.h"
#include "call_so.h"
#include "write_log.h"

int main(int argc, char* argv[]) {
    bool debug = is_debug(argc, argv);

    std::filesystem::path exe_path;
    try {
        exe_path = std::filesystem::canonical(argv[0]);
    } catch (const std::exception& e) {
        std::cerr << "Get exe path failed: " << e.what() << std::endl;
        return 1;
    }

    std::string exe_dir_str = exe_path.parent_path().string();
    // 日志输出到 bin/log
    std::string log_dir = exe_dir_str + "/log";
    init_error_file_log(log_dir);

    std::filesystem::path so_path = exe_path.parent_path() / "lib" / "libinitialization.so";
    call_so(so_path.c_str(), "initialization", debug, exe_dir_str.c_str());

    return 0;
}