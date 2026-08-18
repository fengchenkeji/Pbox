#pragma once
#include <vector>
#include <string>

struct MirrorSource
{
    std::string name;
    std::string base_url;
};

std::vector<MirrorSource> get_mirror_list();

bool download_rootfs(const std::string& relative_path,
                    const std::string& save_path,
                    bool debug);

bool extract_rootfs(const std::string& tar_path,
                    const std::string& rootfs_dir,
                    const std::string& arch,
                    bool debug);
