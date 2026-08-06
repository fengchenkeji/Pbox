#pragma once
#include <memory>
#include <spdlog/spdlog.h>

bool is_debug(int argc, char* argv[]);
std::shared_ptr<spdlog::logger> get_console_logger();

// 宏参数：logger、是否debug、日志内容
#define DBG(logger, debug_flag, ...) \
do{if((debug_flag) && (logger)) SPDLOG_LOGGER_INFO(logger,__VA_ARGS__);}while(0)

void call_so(const char* so_path, const char* func_name, bool debug, const char* exe_dir);