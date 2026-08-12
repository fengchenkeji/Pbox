#include "image_db.h"
#include "write_log.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <spdlog/spdlog.h>
#include <unordered_set>

std::string ImageDb::to_lower(std::string s)
{
    for (char& c : s)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

std::string ImageDb::trim(const std::string &s)
{
    auto start = s.begin();
    while (start != s.end() && std::isspace(static_cast<unsigned char>(*start)))
        start++;

    auto end = s.end();
    do
    {
        end--;
    } while (std::distance(start, end) > 0 && std::isspace(static_cast<unsigned char>(*end)));

    return std::string(start, end + 1);
}

bool ImageDb::load(const std::string& filepath, bool debug)
{
    auto logger = get_console_logger();
    m_images.clear();
    std::ifstream file(filepath);
    if (!file.is_open())
    {
        DBG(logger, debug, "打开镜像文件失败:{}", filepath);
        return false;
    }
    std::string line;
    while (std::getline(file, line))
    {
        if (line.empty()) continue;
        std::stringstream ss(line);
        ImageInfo info;
        std::string os, rel, arch, var, path;
        std::getline(ss, os, '|');
        std::getline(ss, rel, '|');
        std::getline(ss, arch, '|');
        std::getline(ss, var, '|');
        std::getline(ss, path);

        info.os = trim(os);
        info.release = trim(rel);
        info.arch = trim(arch);
        info.variant = trim(var);
        info.path = trim(path);
        m_images.push_back(info);
    }
    file.close();
    DBG(logger, debug, "加载镜像记录总数:{}", m_images.size());
    return !m_images.empty();
}

void ImageDb::get_all_os(std::vector<std::string>& out_os) const
{
    out_os.clear();
    std::unordered_set<std::string> os_set;
    for (const auto& item : m_images)
    {
        os_set.insert(item.os);
    }
    out_os.assign(os_set.begin(), os_set.end());
}

std::vector<std::string> ImageDb::get_releases(const std::string& os) const
{
    std::vector<std::string> res;
    std::string target_os = to_lower(os);
    std::unordered_set<std::string> ver_set;
    for (const auto& item : m_images)
    {
        if (to_lower(item.os) == target_os)
        {
            ver_set.insert(item.release);
        }
    }
    res.assign(ver_set.begin(), ver_set.end());
    return res;
}

std::vector<std::string> ImageDb::get_archs(const std::string& os, const std::string& release) const
{
    std::vector<std::string> res;
    std::string t_os = to_lower(os);
    std::string t_rel = to_lower(release);
    std::unordered_set<std::string> arch_set;
    for (const auto& item : m_images)
    {
        if (to_lower(item.os) == t_os && to_lower(item.release) == t_rel)
        {
            arch_set.insert(item.arch);
        }
    }
    res.assign(arch_set.begin(), arch_set.end());
    return res;
}

std::string ImageDb::get_image_path(const std::string& os, const std::string& release, const std::string& arch, const std::string& variant) const
{
    std::string t_os = to_lower(os);
    std::string t_rel = to_lower(release);
    std::string t_arch = to_lower(arch);
    std::string t_var = to_lower(variant);
    for (const auto& item : m_images)
    {
        if (to_lower(item.os) == t_os
            && to_lower(item.release) == t_rel
            && to_lower(item.arch) == t_arch
            && to_lower(item.variant) == t_var)
        {
            return item.path;
        }
    }
    return "";
}
