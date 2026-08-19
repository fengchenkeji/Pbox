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
