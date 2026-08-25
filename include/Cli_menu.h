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
#include <vector>
#include <string>
#include "write_log.h"

class ImageDb;

enum class SubCmdType
{
    None,
    Help,
    List,
    Install,
    Login
};

struct CliParseResult
{
    SubCmdType cmd = SubCmdType::None;
    bool debug = false;
    std::string list_arg;
    std::string install_arg;
};

CliParseResult parse_cli(int argc, char* argv[]);
int execute_command(const CliParseResult& res, ImageDb& db, const std::string& exe_dir);

// 启动 proot 容器：内部加载 libinitialization.so 并调用 run_proot
int start_proot(const std::string& rootfs_path,
                const std::string& exe_dir,
                LoggerPtr logger,
                bool debug);
