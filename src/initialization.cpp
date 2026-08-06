#include "initialization.h"
#include "call_so.h"
#include "image_db.h"
#include <iostream>
#include <ostream>
#include <string>
#include <vector>

static ImageDb g_db;

std::vector<std::string> get_information(){
    std::vector<std::string> All_Os;
    g_db.get_all_os(All_Os);
    return All_Os;
}

extern "C" int initialization(const char* exe_dir, bool debug)
{
    auto logger = get_console_logger();

    // 必须传入debug，只有debug=true才打印
    DBG(logger, debug, "received exe_dir: [{}]", std::string(exe_dir));

    std::string img_path = std::string(exe_dir) + "/res/images.txt";
    DBG(logger, debug, "final file path: [{}]", img_path);

    bool ok = g_db.load(img_path,debug);

    if(!ok){
        // 错误日志始终打印，不受debug控制
        SPDLOG_LOGGER_ERROR(logger,"init image db failed");
        return -1;
    }
    std::vector<std::string> Os_list = get_information();
    for (const auto& os : Os_list)
         {
             std::cout << os << std::endl;
         }
    return 0;

}
