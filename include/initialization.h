#pragma once
#include "container_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 启动 proot 容器（基于 tmoe/andronix 启动脚本逻辑）
 *
 * 参数:
 *   exe_dir      程序运行目录（用于定位 lib/libproot.so）
 *   rootfs_path  rootfs 根目录绝对路径
 *   config       容器配置（绑定挂载 + 端口转发）
 *
 * 返回码:
 *   -1  dlopen 打开 libproot.so 失败
 *   -2  dlsym 查找 main 符号失败
 *   >=0 proot 程序正常退出返回值
 *
 * 启动参数等价于:
 *   proot --link2symlink -0 -r <rootfs> \
 *     -b /dev -b /proc -b <rootfs>/root:/dev/shm \
 *     [-b <host>:<container> ...] \
 *     -w /root \
 *     /usr/bin/env -i HOME=/root PATH=... TERM=$TERM LANG=C.UTF-8 \
 *     /bin/bash --login
 */
int run_proot(const char* exe_dir, const char* rootfs_path, const ContainerConfig* config);

#ifdef __cplusplus
}
#endif
