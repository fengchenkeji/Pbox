#pragma once
#include <memory>
#include <spdlog/spdlog.h>

void init_error_file_log(const std::string& log_dir);
std::shared_ptr<spdlog::logger> get_console_logger();

using LoggerPtr = std::shared_ptr<spdlog::logger>;

// 统一调试日志宏
#define DBG(log_obj, enable, fmt, ...)                          \
    do {                                                        \
        if ((enable) && log_obj) {                              \
            log_obj->info(fmt, ##__VA_ARGS__);                  \
        }                                                       \
    } while (0)
