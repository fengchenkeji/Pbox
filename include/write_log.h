#pragma once
#include <spdlog/spdlog.h>
#include <memory>

extern std::shared_ptr<spdlog::logger> get_console_logger();
using LoggerPtr = std::shared_ptr<spdlog::logger>;

#define DBG(logger, debug, fmt, ...) \
do{ if(debug && logger) logger->info(fmt, ##__VA_ARGS__); }while(0)
