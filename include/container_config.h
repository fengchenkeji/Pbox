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

// 端口转发规则
struct PortForward
{
    int hostPort = 0;
    int containerPort = 0;
};

// 容器配置：绑定挂载 + 端口转发
struct ContainerConfig
{
    std::vector<std::string> mounts;       // 每项格式: 宿主路径:容器路径
    std::vector<PortForward> portForwards;
};

/**
 * 从两处加载容器配置并合并：
 *   1. <rootfs_dir>/config/<tag>
 *   2. <exe_dir>/proot/start_script/<tag>
 * 配置行格式:
 *   mnt += /storage/emulated/0/ -b    绑定挂载到容器 /mnt
 *   22  += 8022 -p                    宿主8022转发到容器22
 */
bool load_container_config(const std::string& exe_dir,
                           const std::string& rootfs_dir,
                           ContainerConfig& cfg,
                           LoggerPtr logger,
                           bool debug);

/**
 * 安装时生成默认配置文件（同时作为安装标记）
 */
bool generate_default_config(const std::string& config_path,
                             const std::string& os_name,
                             const std::string& release,
                             const std::string& arch,
                             LoggerPtr logger,
                             bool debug);
