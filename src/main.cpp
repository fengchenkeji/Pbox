#include <iostream>
#include <string>
#include <filesystem>
#include "../include/initialization.h"
#include "../include/call_so.h"

int main(int argc, char* argv[]) {
    bool debug = is_debug(argc, argv);

    std::filesystem::path exe_path;
    try {
        exe_path = std::filesystem::canonical(argv[0]);
    } catch (const std::exception& e) {
        std::cerr << "Get exe path failed: " << e.what() << std::endl;
        return 1;
    }

    std::filesystem::path so_path = exe_path.parent_path() / "lib" / "libinitialization.so";
    call_so(so_path.c_str(), "initialization", debug);

    return 0;
}
