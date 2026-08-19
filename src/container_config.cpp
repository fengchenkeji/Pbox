#include "container_config.h"
#include "write_log.h"

#include <fstream>
#include <sstream>
#include <filesystem>
#include <cstdlib>

namespace fs = std::filesystem;
std::shared_ptr<spdlog::logger> get_console_logger();

// 去除字符串首尾空白
static std::string trim(const std::string& s)
{
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

// 确保容器内路径以 / 开头
static std::string normalize_container_path(const std::string& p)
{
    if (p.empty()) return "/";
    if (p[0] == '/') return p;
    return "/" + p;
}

bool parse_config_line(const std::string& raw_line, ContainerConfig& out, bool debug)
{
    auto logger = get_console_logger();
    std::string line = trim(raw_line);

    // 空行或注释
    if (line.empty() || line[0] == '#')
        return true;

    // 查找 " += " 分隔符
    size_t sep_pos = line.find("+=");
    if (sep_pos == std::string::npos)
    {
        DBG(logger, debug, "配置行格式错误(缺少+=): {}", line);
        return false;
    }

    std::string left = trim(line.substr(0, sep_pos));
    std::string right_part = trim(line.substr(sep_pos + 2));

    if (left.empty() || right_part.empty())
    {
        DBG(logger, debug, "配置行左右侧为空: {}", line);
        return false;
    }

    // right_part 末尾是 flag (-b / -p)，前面是 source
    size_t last_space = right_part.find_last_of(" \t");
    if (last_space == std::string::npos)
    {
        DBG(logger, debug, "配置行缺少flag(-b/-p): {}", line);
        return false;
    }

    std::string source = trim(right_part.substr(0, last_space));
    std::string flag = trim(right_part.substr(last_space + 1));

    if (flag == "-b")
    {
        // 绑定挂载：left=容器挂载点, source=宿主机路径
        BindRule rule;
        rule.host_path = source;
        rule.container_path = normalize_container_path(left);
        out.binds.push_back(rule);
        DBG(logger, debug, "解析绑定挂载: {} -> {}", rule.host_path, rule.container_path);
        return true;
    }
    else if (flag == "-p")
    {
        // 端口转发：left=容器端口, source=宿主机端口
        PortRule rule;
        rule.container_port = std::atoi(left.c_str());
        rule.host_port = std::atoi(source.c_str());
        if (rule.container_port <= 0 || rule.host_port <= 0)
        {
            DBG(logger, debug, "端口号无效: {}", line);
            return false;
        }
        out.ports.push_back(rule);
        DBG(logger, debug, "解析端口转发: 宿主{} -> 容器{}", rule.host_port, rule.container_port);
        return true;
    }

    DBG(logger, debug, "未知flag: {}", flag);
    return false;
}

// 从单个文件加载配置
static bool load_config_file(const std::string& filepath, ContainerConfig& out, bool debug)
{
    auto logger = get_console_logger();
    if (!fs::exists(filepath) || !fs::is_regular_file(filepath))
    {
        return false;
    }

    std::ifstream f(filepath);
    if (!f.is_open())
    {
        DBG(logger, debug, "无法打开配置文件: {}", filepath);
        return false;
    }

    std::string line;
    bool has_valid = false;
    while (std::getline(f, line))
    {
        // 跳过安装标记行（installed xxx / arch=xxx）
        std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == '#' ||
            trimmed.substr(0, 9) == "installed" ||
            trimmed.substr(0, 5) == "arch=")
        {
            continue;
        }
        if (parse_config_line(line, out, debug))
        {
            has_valid = true;
        }
    }
    f.close();

    if (has_valid)
    {
        out.valid = true;
        DBG(logger, debug, "从配置文件加载成功: {} (绑定{}条, 端口{}条)",
            filepath, out.binds.size(), out.ports.size());
    }
    return has_valid;
}

bool load_container_config(const std::string& start_script_file,
                           const std::string& rootfs_dir,
                           const std::string& tag,
                           ContainerConfig& out,
                           bool debug)
{
    auto logger = get_console_logger();

    // 先读 rootfs 内 config 目录（容器级默认配置）
    std::string rootfs_config = fs::path(rootfs_dir) / "config" / tag;
    bool rootfs_loaded = load_config_file(rootfs_config, out, debug);

    // 再读 start_script 文件（用户级配置，可覆盖/追加）
    bool script_loaded = load_config_file(start_script_file, out, debug);

    if (rootfs_loaded || script_loaded)
    {
        out.valid = true;
    }

    DBG(logger, debug, "容器配置加载完成: rootfs_config={}, start_script={}, 绑定={}, 端口={}",
        rootfs_loaded, script_loaded, out.binds.size(), out.ports.size());

    return out.valid;
}

bool generate_default_config(const std::string& start_script_file, bool debug)
{
    auto logger = get_console_logger();
    try
    {
        fs::path p(start_script_file);
        fs::create_directories(p.parent_path());

        std::ofstream f(start_script_file, std::ios::app);
        if (!f.is_open())
        {
            DBG(logger, debug, "无法写入默认配置: {}", start_script_file);
            return false;
        }

        // 写入默认配置注释和示例
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
        f.close();

        DBG(logger, debug, "默认配置已生成: {}", start_script_file);
        return true;
    }
    catch (const std::exception& e)
    {
        DBG(logger, debug, "生成默认配置异常: {}", e.what());
        return false;
    }
}
