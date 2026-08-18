#include "download_rootfs.hpp"
#include "write_log.h"
#include <spdlog/spdlog.h>
#include <cstdlib>
#include <iostream>
#include <filesystem>
#include <archive.h>
#include <archive_entry.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

namespace fs = std::filesystem;

std::vector<MirrorSource> get_mirror_list()
{
    return {
        {"清华镜像源", "https://mirrors.tuna.tsinghua.edu.cn/lxc-images"},
        {"南阳理工镜像源", "https://mirror.nyist.edu.cn/lxc-images"},
        {"网易镜像源", "https://mirrors.163.com/lxc-images"},
        {"阿里云镜像源", "https://mirrors.aliyun.com/lxc-images"},
        {"LXC官方源", "https://images.linuxcontainers.org"}
    };
}

bool download_rootfs(const std::string& relative_path,
                    const std::string& save_path,
                    bool debug)
{
    auto logger = get_console_logger();
    auto mirrors = get_mirror_list();

    for (const auto& mirror : mirrors)
    {
        std::string url = mirror.base_url;
        if (!relative_path.empty() && relative_path.front() != '/')
            url += "/";
        url += relative_path;

        DBG(logger, debug, "尝试从【{}】下载: {}", mirror.name, url);
        std::string cmd = "wget -q --timeout=30 --tries=2 -O " + save_path + " " + url;
        int ret = system(cmd.c_str());

        if (ret == 0 && fs::exists(save_path) && fs::file_size(save_path) > 0)
        {
            DBG(logger, debug, "【{}】下载成功", mirror.name);
            return true;
        }
        DBG(logger, debug, "【{}】下载失败，切换下一镜像源", mirror.name);
        if (fs::exists(save_path))
            fs::remove(save_path);
    }
    std::cerr << "[错误] 所有镜像源下载rootfs压缩包失败，请检查网络或稍后重试\n";
    DBG(logger, debug, "全部镜像源下载rootfs失败");
    return false;
}

/**
 * 解压完成后：手动补 proot 启动必须的核心软链接
 * 策略：不恢复 tar 内全部符号链接（避免 Android 权限崩溃），只补启动最低限度链接
 *
 * 关键点：
 * 1. usrmerge 根目录链接 /bin /lib /sbin 必须用【容器内绝对路径】字符串，不能用相对路径
 * 2. armhf: ld-linux-armhf.so.3 本身是实体 ELF 文件，禁止创建/覆盖其软链接
 * 3. arm64/amd64: ld-xxx.so 是指向 ld-2.xx.so 的符号链接，需要手动创建
 */
static bool fix_minimal_symlinks(const std::string& rootfs_dir, const std::string& arch, bool debug)
{
    auto logger = get_console_logger();
    std::vector<std::pair<std::string, std::string>> link_list;

    // ===== usrmerge 根目录核心软链接（debian/ubuntu 必备）=====
    // 第二个参数是写入软链接内部的字符串，必须是容器内绝对路径（以 / 开头）
    link_list.emplace_back(rootfs_dir + "/bin", "/usr/bin");
    link_list.emplace_back(rootfs_dir + "/lib", "/usr/lib");
    link_list.emplace_back(rootfs_dir + "/sbin", "/usr/sbin");

    // ===== 架构相关：动态链接器软链接 =====
    if (arch == "armhf" || arch == "armel")
    {
        // armhf 特殊：usr/lib/arm-linux-gnueabihf/ld-linux-armhf.so.3 是真实实体文件
        // 不需要创建任何 ld 相关软链接，此处留空
        DBG(logger, debug, "armhf架构：ld-linux-armhf.so.3为实体文件，跳过ld软链接创建");
    }
    else if (arch == "arm64" || arch == "aarch64")
    {
        fs::path lib_dir = fs::path(rootfs_dir) / "usr/lib/aarch64-linux-gnu";
        std::error_code ec;
        fs::create_directories(lib_dir, ec);
        link_list.emplace_back(rootfs_dir + "/lib64", "/usr/lib");
        link_list.emplace_back(lib_dir.string() + "/ld-linux-aarch64.so.1", "ld-2.35.so");
    }
    else if (arch == "amd64" || arch == "x86_64")
    {
        fs::path lib_dir = fs::path(rootfs_dir) / "usr/lib/x86_64-linux-gnu";
        std::error_code ec;
        fs::create_directories(lib_dir, ec);
        link_list.emplace_back(rootfs_dir + "/lib64", "/usr/lib");
        link_list.emplace_back(lib_dir.string() + "/ld-linux-x86-64.so.2", "ld-2.35.so");
    }

    // ===== 通用工具软链接 =====
    link_list.emplace_back(rootfs_dir + "/bin/sh", "/usr/bin/bash");

    for (auto &item : link_list)
    {
        const std::string& dst = item.first;
        const std::string& target = item.second;

        // 已存在则跳过（不覆盖，避免破坏实体文件）
        if (fs::exists(dst))
        {
            DBG(logger, debug, "软链接已存在，跳过: {}", dst);
            continue;
        }

        // 确保父目录存在
        fs::path parent = fs::path(dst).parent_path();
        std::error_code ec;
        fs::create_directories(parent, ec);

        int rc = symlink(target.c_str(), dst.c_str());
        if (rc != 0)
        {
            DBG(logger, debug, "创建软链接失败 {} -> {} errno={}", dst, target, errno);
        }
        else
        {
            DBG(logger, debug, "创建软链接 {} -> {}", dst, target);
        }
    }
    return true;
}

/**
 * tar解压：
 * - 普通文件：写入，还原tar原始mode权限（保留可执行位）
 * - 目录：创建，还原目录mode
 * - symlink/设备/fifo：全部跳过，不从tar读取（避免Android权限崩溃）
 * - 权限异常直接跳过该条目，不终止整体流程
 * - strip-components=1：剥离tar第一层目录
 * @param arch 镜像架构 armhf / aarch64 / amd64，用于补ld软链接
 */
bool extract_rootfs(const std::string& tar_path,
                    const std::string& rootfs_dir,
                    const std::string& arch,
                    bool debug)
{
    auto logger = get_console_logger();
    if (!fs::exists(tar_path))
    {
        std::cerr << "[错误] 压缩包不存在，解压失败\n";
        DBG(logger, debug, "压缩包不存在: {}", tar_path);
        return false;
    }

    try
    {
        fs::create_directories(rootfs_dir);
    }
    catch (const fs::filesystem_error& e)
    {
        std::cerr << "[错误] 创建rootfs根目录权限失败: " << e.what() << "\n";
        DBG(logger, debug, "创建rootfs目录异常:{}", e.what());
        return false;
    }

    struct archive* a = archive_read_new();
    struct archive_entry* entry;
    archive_read_support_filter_xz(a);
    archive_read_support_format_tar(a);

    int r = archive_read_open_filename(a, tar_path.c_str(), 10240);
    if (r != ARCHIVE_OK)
    {
        std::cerr << "[错误] 打开压缩包失败: " << archive_error_string(a) << "\n";
        DBG(logger, debug, "打开tar.xz失败:{}", archive_error_string(a));
        archive_read_free(a);
        return false;
    }

    bool has_valid_file = false;
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK)
    {
        std::string entry_path = archive_entry_pathname(entry);

        // strip-components=1，剥离tar第一层目录
        size_t slash_pos = entry_path.find('/');
        if (slash_pos != std::string::npos)
            entry_path = entry_path.substr(slash_pos + 1);
        if (entry_path.empty())
        {
            archive_read_data_skip(a);
            continue;
        }

        fs::path out_path = fs::path(rootfs_dir) / entry_path;
        auto entry_type = archive_entry_filetype(entry);

        // tar包内符号链接、设备、FIFO：全部跳过，不从归档解压
        if (entry_type != AE_IFREG && entry_type != AE_IFDIR)
        {
            archive_read_data_skip(a);
            continue;
        }

        try
        {
            fs::path parent = out_path.parent_path();
            fs::create_directories(parent);

            if (entry_type == AE_IFREG)
            {
                FILE* out_fp = fopen(out_path.c_str(), "wb");
                if (out_fp != nullptr)
                {
                    char buf[8192];
                    la_ssize_t len;
                    while ((len = archive_read_data(a, buf, sizeof(buf))) > 0)
                    {
                        fwrite(buf, 1, len, out_fp);
                    }
                    fclose(out_fp);

                    // 还原tar归档内原始文件权限，保留可执行位
                    // & 0777 屏蔽 setuid/setgid 高位，Android不支持
                    mode_t file_mode = archive_entry_mode(entry);
                    chmod(out_path.c_str(), file_mode & 0777);

                    has_valid_file = true;
                }
                else
                {
                    // open失败，权限不足，跳过该文件
                    DBG(logger, debug, "跳过无权限文件:{}", out_path.string());
                    archive_read_data_skip(a);
                    continue;
                }
            }
            else if (entry_type == AE_IFDIR)
            {
                fs::create_directories(out_path);
                // 还原目录权限
                mode_t dir_mode = archive_entry_mode(entry);
                chmod(out_path.c_str(), dir_mode & 0777);
            }
        }
        catch (const fs::filesystem_error& e)
        {
            // 权限/不支持操作，跳过当前条目，继续解压其他
            DBG(logger, debug, "跳过权限异常条目 {} : {}", out_path.string(), e.what());
            archive_read_data_skip(a);
            continue;
        }

        archive_read_data_skip(a);
    }

    archive_read_close(a);
    archive_read_free(a);

    if (!has_valid_file)
    {
        std::cerr << "[错误] 压缩包内无有效系统文件\n";
        DBG(logger, debug, "解压后无有效文件");
        try {
            fs::remove_all(rootfs_dir);
        } catch (...) {}
        return false;
    }

    DBG(logger, debug, "纯C++解压完成，目录:{}", rootfs_dir);
    // 解压结束，根据架构补必须的动态链接器和usrmerge软链接
    fix_minimal_symlinks(rootfs_dir, arch, debug);
    return true;
}
