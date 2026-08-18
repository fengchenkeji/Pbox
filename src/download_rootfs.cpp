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
        {"本地",},
        {"清华镜像源", "https://mirrors.tuna.tsinghua.edu.cn/lxc-images"},
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

// 解压完成后：手动补proot启动必须核心软链接，不读取tar内symlink
static bool fix_minimal_symlinks(const std::string& rootfs_dir, const std::string& arch, bool debug)
{
    auto logger = get_console_logger();
    std::vector<std::pair<std::string, std::string>> link_list;

    if(arch == "armhf")
    {
        link_list.emplace_back(rootfs_dir + "/lib/ld-linux.so.3", "arm-linux-gnueabihf/ld-2.35.so");
    }
    else if(arch == "arm64" || arch == "aarch64")
    {
        link_list.emplace_back(rootfs_dir + "/lib/aarch64-linux-gnu/ld-linux-aarch64.so.1", "ld-2.35.so");
    }
    // 通用工具软链接
    link_list.emplace_back(rootfs_dir + "/usr/bin/perl", "perl5.34.0");
    link_list.emplace_back(rootfs_dir + "/usr/bin/uncompress", "gunzip");

    for(auto &item : link_list)
    {
        const std::string& dst = item.first;
        const std::string& target = item.second;
        if(fs::exists(dst))
            continue;
        int rc = symlink(target.c_str(), dst.c_str());
        if(rc != 0)
        {
            DBG(logger, debug, "创建必要软链接失败 {} → {} errno={}", dst, target, errno);
        }
        else
        {
            DBG(logger, debug, "创建必要软链接 {} → {}", dst, target);
        }
    }
    return true;
}

/**
 * tar解压：
 * - 普通文件：写入，还原tar原始mode权限
 * - 目录：创建，还原目录mode
 * - symlink/设备/fifo：全部跳过，不从tar读取
 * - 权限异常直接跳过该条目，不解压，不终止整体流程
 * @param arch 镜像架构 armhf / aarch64，用于补ld软链接
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

        // strip‑components=1，剥离tar第一层目录
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
    // 解压结束，根据架构补必须的动态链接器软链接
    fix_minimal_symlinks(rootfs_dir, arch, debug);
    return true;
}
