#include "../include/initialization.h"
#include "../include/image_db.h"
#include "../include/call_so.h"
#include <spdlog/logger.h>
#include <string>

// 全局静态镜像数据库，初始化后全局可用
static ImageDb g_image_db;

// 对外获取全局数据库实例，其他文件可调用查询
ImageDb& get_global_image_db() {
    return g_image_db;
}

extern "C" int initialization(const char* exe_dir, bool debug) {
    // 修正路径拼接，强制加目录分隔符
    std::string images_path = std::string(exe_dir) + "/res/images.txt";

    auto logger = get_console_logger();

    // 加载镜像数据库
    if (g_image_db.load(images_path)) {
        if (debug) {
            DBG(logger, "Loading mirror file success: {}", images_path);
        }
        return 0; // 初始化成功返回 0
    } else {
        SPDLOG_LOGGER_ERROR(logger, "Loading mirror file failed: {}", images_path);
        return -1; // 加载失败返回错误码
    }
}
