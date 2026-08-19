#pragma once
#include <vector>
#include <string>

// 绑定挂载规则：把宿主机 host_path 挂载到容器内 container_path
struct BindRule
{
    std::string host_path;       // 宿主机路径，如 /storage/emulated/0/
    std::string container_path;  // 容器内挂载点，如 /mnt
};

// 端口转发规则：宿主机 host_port 转发到容器 container_port
// proot 共享宿主机网络，通过 socat 后台进程实现
struct PortRule
{
    int container_port = 0;  // 容器内端口，如 22
    int host_port = 0;       // 宿主机端口，如 8022
};

// 容器完整配置（合并 start_script 和 rootfs/config 两处）
struct ContainerConfig
{
    std::vector<BindRule> binds;
    std::vector<PortRule> ports;
    bool valid = false;  // 至少成功读取了一个配置文件
};

/**
 * 解析配置文件行格式：
 *   <container_side> += <host_side> <flag>
 *   flag = -b  绑定挂载：container_side=容器挂载点, host_side=宿主机路径
 *   flag = -p  端口转发：container_side=容器端口,   host_side=宿主机端口
 * 示例：
 *   mnt += /storage/emulated/0/ -b
 *   22  += 8022 -p
 * 以 # 开头的行为注释，空行跳过。
 */
bool parse_config_line(const std::string& line, ContainerConfig& out, bool debug);

/**
 * 从两个位置加载配置并合并：
 *   1. start_script 文件：<exe_dir>/proot/start_script/<tag>
 *   2. rootfs 内 config：<rootfs_dir>/config/<tag>
 * 两处规则合并，start_script 优先（先读 rootfs/config，再读 start_script 覆盖/追加）。
 */
bool load_container_config(const std::string& start_script_file,
                           const std::string& rootfs_dir,
                           const std::string& tag,
                           ContainerConfig& out,
                           bool debug);

/**
 * 安装容器时生成默认配置文件到 start_script 目录。
 * 默认包含：挂载 /storage/emulated/0/ 到 /mnt
 */
bool generate_default_config(const std::string& start_script_file, bool debug);
