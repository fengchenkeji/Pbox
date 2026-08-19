#include "container_config.h"

#include <fstream>
#include <sstream>
#include <filesystem>
#include <cstdlib>

namespace fs = std::filesystem;

// 去除首尾空白
static std::string trim(const std::string& s)
{
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

// 解析单行配置
static void parse_line(const std::string& raw_line, ContainerConfig& cfg, LoggerPtr logger, bool debug)
{
    std::string line = trim(raw_line);
    if (line.empty() || line[0] == '#') return;
    // 跳过安装标记行
    if (line.substr(0, 9) == "installed" || line.substr(0, 5) == "arch=") return;

    size_t sep = line.find("+=");
    if (sep == std::string::npos) return;

    std::string left = trim(line.substr(0, sep));
    std::string right = trim(line.substr(sep + 2));
    if (left.empty() || right.empty()) return;

    // right 末尾是 flag (-b / -p)
    size_t last_sp = right.find_last_of(" \t");
    if (last_sp == std::string::npos) return;

    std::string source = trim(right.substr(0, last_sp));
    std::string flag = trim(right.substr(last_sp + 1));

    if (flag == "-b")
    {
        // 绑定挂载: left=容器挂载点(如mnt), source=宿主路径
        std::string container_path = (left[0] == '/') ? left : "/" + left;
        std::string mount_str = source + ":" + container_path;
        cfg.mounts.push_back(mount_str);
        DBG(logger, debug, "解析绑定挂载: {}", mount_str);
    }
    else if (flag == "-p")
    {
        // 端口转发: left=容器端口, source=宿主端口
        PortForward pf;
        pf.containerPort = std::atoi(left.c_str());
        pf.hostPort = std::atoi(source.c_str());
        if (pf.containerPort > 0 && pf.hostPort > 0)
        {
            cfg.portForwards.push_back(pf);
            DBG(logger, debug, "解析端口转发: 宿主{} -> 容器{}", pf.hostPort, pf.containerPort);
        }
    }
}

// 从单个文件加载配置
static bool load_file(const std::string& path, ContainerConfig& cfg, LoggerPtr logger, bool debug)
{
    if (!fs::exists(path) || !fs::is_regular_file(path)) return false;

    std::ifstream f(path);
    if (!f.is_open()) return false;

    std::string line;
    bool has_valid = false;
    while (std::getline(f, line))
    {
        size_t before = cfg.mounts.size() + cfg.portForwards.size();
        parse_line(line, cfg, logger, debug);
        if (cfg.mounts.size() + cfg.portForwards.size() > before)
            has_valid = true;
    }
    f.close();

    if (has_valid)
        DBG(logger, debug, "从配置文件加载: {} (绑定{}条, 端口{}条)",
            path, cfg.mounts.size(), cfg.portForwards.size());
    return has_valid;
}

bool load_container_config(const std::string& exe_dir,
                           const std::string& rootfs_dir,
                           ContainerConfig& cfg,
                           LoggerPtr logger,
                           bool debug)
{
    // 从 rootfs_dir 反推 tag
    fs::path rp(rootfs_dir);
    std::string tag = rp.filename().string();

    // 1. rootfs 内 config
    std::string rootfs_cfg = rootfs_dir + "/config/" + tag;
    bool ok1 = load_file(rootfs_cfg, cfg, logger, debug);

    // 2. start_script 文件
    std::string script_cfg = exe_dir + "/proot/start_script/" + tag;
    bool ok2 = load_file(script_cfg, cfg, logger, debug);

    DBG(logger, debug, "容器配置加载完成: rootfs={}, start_script={}, 绑定={}, 端口={}",
        ok1, ok2, cfg.mounts.size(), cfg.portForwards.size());
    return true; // 无配置也返回成功，使用默认系统挂载
}

bool generate_default_config(const std::string& config_path,
                             const std::string& os_name,
                             const std::string& release,
                             const std::string& arch,
                             LoggerPtr logger,
                             bool debug)
{
    try
    {
        fs::path p(config_path);
        fs::create_directories(p.parent_path());

        std::ofstream f(config_path);
        if (!f.is_open())
        {
            DBG(logger, debug, "无法写入配置文件: {}", config_path);
            return false;
        }

        f << "installed " << os_name << ":" << release << "\n";
        f << "arch=" << arch << "\n";
        f << "\n";
        f << "# ===== Pbox 容器配置 =====\n";
        f << "# 格式: <容器侧> += <宿主侧> <类型>\n";
        f << "#   -b  绑定挂载: 容器挂载点 += 宿主机路径 -b\n";
        f << "#   -p  端口转发: 容器端口 += 宿主机端口 -p\n";
        f << "# 示例:\n";
        f << "#   mnt += /storage/emulated/0/ -b\n";
        f << "#   22 += 8022 -p\n";
        f << "\n";
        f << "# 默认挂载手机内部存储到容器 /mnt\n";
        f << "mnt += /storage/emulated/0/ -b\n";
        f << "\n";
        f << "# 端口转发示例（取消注释生效，需安装 socat）:\n";
        f << "# 22 += 8022 -p\n";
        f.close();

        DBG(logger, debug, "默认配置已生成: {}", config_path);
        return true;
    }
    catch (const std::exception& e)
    {
        DBG(logger, debug, "生成默认配置异常: {}", e.what());
        return false;
    }
}
