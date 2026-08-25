/*
 * Pbox - Termux proot container manager
 * Copyright (C) 2026  fengchenkeji
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "write_log.h"
#include <iostream>
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
    std::string cmd = "mkdir -p " + log_dir;
    std::system(cmd.c_str());

    std::string log_file_path = log_dir + "/" + get_today_date() + ".log";

    if (spdlog::get("console"))
    {
        return;
    }

    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::trace);

    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_file_path, false);
    file_sink->set_level(spdlog::level::err);

    std::string log_fmt = "[%Y-%m-%d %H:%M:%S.%e] [%l] [%s:%#] %v";
    console_sink->set_pattern(log_fmt);
    file_sink->set_pattern(log_fmt);

    std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
    auto logger = std::make_shared<spdlog::logger>("console", sinks.begin(), sinks.end());

    logger->set_level(spdlog::level::trace);
    logger->flush_on(spdlog::level::err);

    spdlog::register_logger(logger);
}

std::shared_ptr<spdlog::logger> get_console_logger()
{
    return spdlog::get("console");
}
