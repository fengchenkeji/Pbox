#include "write_log.h"
#include <iostream>
#include <iterator>
#include <ostream>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <cstdlib>

namespace
{
static std::string get_today_date()
{
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&t);

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d");
    return oss.str();
}
}

void init_error_file_log(const std::string& log_dir)
{
    // 创建日志文件夹
    std::string cmd = "mkdir -p " + log_dir;
    // std::cout<<log_dir<<std::endl;
    std::system(cmd.c_str());

    std::string log_file_path = log_dir + "/" + get_today_date() + ".log";

    // 防止重复初始化
    if (spdlog::get("console"))
    {
        return;
    }

    // 1. 控制台sink：打印全部日志级别
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::trace);

    // 2. 文件sink：仅保存error及以上
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_file_path, false);
    file_sink->set_level(spdlog::level::err);

    // 统一日志格式
    std::string log_fmt = "[%Y-%m-%d %H:%M:%S.%e] [%l] [%s:%#] %v";
    console_sink->set_pattern(log_fmt);
    file_sink->set_pattern(log_fmt);

    // 组装双输出sink
    std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
    auto logger = std::make_shared<spdlog::logger>("console", sinks.begin(), sinks.end());

    logger->set_level(spdlog::level::trace);
    // 遇到error立刻刷文件
    logger->flush_on(spdlog::level::err);

    spdlog::register_logger(logger);
}
