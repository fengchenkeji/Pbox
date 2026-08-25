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
#include "image_db.h"
#include "write_log.h"
#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <vector>
#include <string>
#include <set>

using json = nlohmann::json;
std::shared_ptr<spdlog::logger> get_console_logger();

struct ImageItem {
    std::string os;
    std::string release;
    std::string arch;
    std::string variant;
    std::string rootfs_path;
};

static std::vector<ImageItem> g_image_list;

// 按分隔符分割字符串
static std::vector<std::string> split_str(const std::string& s, char delim)
{
    std::vector<std::string> res;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        res.push_back(item);
    }
    return res;
}

bool ImageDb::load(const std::string& json_path, bool debug)
{
    auto logger = get_console_logger();
    std::ifstream fin(json_path);
    if (!fin.is_open()) {
        DBG(logger, debug, "无法打开镜像文件:{}", json_path);
        return false;
    }

    json root;
    try {
        fin >> root;
    } catch (const std::exception& e) {
        DBG(logger, debug, "JSON解析失败:{}", e.what());
        return false;
    }

    if (!root.contains("products") || !root["products"].is_object()) {
        DBG(logger, debug, "JSON格式错误，缺少products根节点");
        return false;
    }

    g_image_list.clear();
    const auto& products = root["products"];
    int valid_count = 0;

    for (auto prod_it = products.begin(); prod_it != products.end(); ++prod_it) {
        std::string product_key = prod_it.key();
        const auto& product_val = prod_it.value();

        auto parts = split_str(product_key, ':');
        if (parts.size() != 4) {
            continue;
        }

        std::string os = parts[0];
        std::string release = parts[1];
        std::string arch = parts[2];
        std::string variant = parts[3];

        if (!product_val.contains("versions") || !product_val["versions"].is_object()) {
            continue;
        }
        const auto& versions = product_val["versions"];
        if (versions.empty()) {
            continue;
        }

        auto latest_ver_it = --versions.end();
        const auto& latest_ver = latest_ver_it.value();

        if (!latest_ver.contains("items") || !latest_ver["items"].is_object()) {
            continue;
        }
        const auto& items = latest_ver["items"];

        if (!items.contains("root.tar.xz")) {
            continue;
        }
        const auto& root_item = items["root.tar.xz"];
        if (!root_item.contains("path")) {
            continue;
        }

        std::string path = root_item["path"].get<std::string>();
        g_image_list.push_back({os, release, arch, variant, path});
        valid_count++;
    }

    DBG(logger, debug, "镜像库加载完成，共{}条镜像记录", valid_count);
    return valid_count > 0;
}

// 获取所有操作系统名称（去重，输出到参数）
void ImageDb::get_all_os(std::vector<std::string>& out_os) const
{
    out_os.clear();
    std::set<std::string> os_set;
    for (const auto& item : g_image_list) {
        os_set.insert(item.os);
    }
    out_os.assign(os_set.begin(), os_set.end());
}

// 根据操作系统获取所有版本（去重，返回vector）
std::vector<std::string> ImageDb::get_releases(const std::string& os) const
{
    std::set<std::string> release_set;
    for (const auto& item : g_image_list) {
        if (item.os == os) {
            release_set.insert(item.release);
        }
    }
    return {release_set.begin(), release_set.end()};
}

// 根据系统+版本获取所有架构（去重，返回vector）
std::vector<std::string> ImageDb::get_archs(const std::string& os, const std::string& release) const
{
    std::set<std::string> arch_set;
    for (const auto& item : g_image_list) {
        if (item.os == os && item.release == release) {
            arch_set.insert(item.arch);
        }
    }
    return {arch_set.begin(), arch_set.end()};
}

// 根据系统、版本、架构、变体获取镜像完整路径
std::string ImageDb::get_image_path(const std::string& os, const std::string& release,
                                    const std::string& arch, const std::string& variant) const
{
    for (const auto& item : g_image_list) {
        if (item.os == os && item.release == release 
            && item.arch == arch && item.variant == variant) {
            return item.rootfs_path;
        }
    }
    return {};
}
