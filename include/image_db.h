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
#include <vector>
#include <string>

struct ImageInfo
{
    std::string os;
    std::string release;
    std::string arch;
    std::string variant;
    std::string path;
};

class ImageDb
{
private:
    std::vector<ImageInfo> m_images;
public:
    // 接口完全不变，仅内部改为解析JSON
    bool load(const std::string& filepath, bool debug);
    void get_all_os(std::vector<std::string>& out_os) const;
    std::vector<std::string> get_releases(const std::string& os) const;
    std::vector<std::string> get_archs(const std::string& os, const std::string& release) const;
    std::string get_image_path(const std::string& os, const std::string& release, const std::string& arch, const std::string& variant) const;
};
