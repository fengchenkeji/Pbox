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
#include "container_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 启动 proot 容器（基于 bash 启动脚本逻辑）
 * 内部加载配置、构建参数、dlopen libproot.so 并调用 main
 *
 * @param exe_dir      程序运行目录
 * @param rootfs_path  rootfs 根目录绝对路径
 * @param logger       日志对象（由主程序传入，so 内部不调用 get_console_logger）
 * @param debug        是否打印调试日志
 * @return -1 dlopen libproot.so 失败; -2 dlsym main 失败; >=0 proot退出码
 */
int run_proot(const char* exe_dir,
              const char* rootfs_path,
              LoggerPtr logger,
              bool debug);

#ifdef __cplusplus
}
#endif
