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
