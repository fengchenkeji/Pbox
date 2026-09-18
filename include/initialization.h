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
 * 启动 proot 容器（直接调用 proot 可执行文件，fork+exec 隔离）
 * 内部加载配置、构建参数，fork 子进程执行 proot 二进制
 *
 * @param exe_dir      程序运行目录
 * @param rootfs_path  rootfs 根目录绝对路径
 * @param logger       日志对象（由主程序传入）
 * @param debug        是否打印调试日志
 * @return >=0 proot退出码; -1 proot 可执行文件未找到; -2 fork 失败
 */
int run_proot(const char* exe_dir,
              const char* rootfs_path,
              LoggerPtr logger,
              bool debug);

/**
 * 获取 proot 版本号
 * 通过执行 proot --version 并解析输出来获取
 *
 * @param exe_dir  程序运行目录（用于查找自带 bin/proot）
 * @param debug    是否打印调试日志
 * @return 版本字符串（如 "5.4.0"），失败返回空字符串
 */
const char* get_proot_version(const char* exe_dir, bool debug);

#ifdef __cplusplus
}
#endif
