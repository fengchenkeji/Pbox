#include "../include/image_db.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include "../include/call_so.h"

// 辅助：按分隔符分割字符串
static std::vector<std::string> split(const std::string& s, const std::string& delim) {
    std::vector<std::string> res;
    size_t pos = 0;
    size_t prev = 0;
    while ((pos = s.find(delim, prev)) != std::string::npos) {
        res.push_back(s.substr(prev, pos - prev));
        prev = pos + delim.size();
    }
    if (prev < s.size()) {
        res.push_back(s.substr(prev));
    }
    return res;
}

bool ImageDb::load(const std::string& filepath) {
    std::ifstream in(filepath);
    if (!in.is_open()) {
        auto logger = get_console_logger();
        SPDLOG_LOGGER_ERROR(logger, "Cannot open image db file: {}", filepath);
        return false;
    }

    m_data.clear();
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;

        // 按 ", " 分割所有键值对
        auto pairs = split(line, ", ");
        ImageInfo info;

        for (const auto& pair : pairs) {
            auto kv = split(pair, ": ");
            if (kv.size() != 2) continue;

            const std::string& key = kv[0];
            const std::string& val = kv[1];

            if (key == "OS") info.os = val;
            else if (key == "Release") info.release = val;
            else if (key == "Arch") info.arch = val;
            else if (key == "Variant") info.variant = val;
            else if (key == "Version") info.version = val;
            else if (key == "Path") info.path = val;
        }

        // 关键字段非空才加入
        if (!info.os.empty() && !info.release.empty() && !info.path.empty()) {
            m_data.push_back(info);
        }
    }

    auto logger = get_console_logger();
    SPDLOG_LOGGER_INFO(logger, "Loaded {} image entries from {}", m_data.size(), filepath);
    return !m_data.empty();
}

std::vector<std::string> ImageDb::get_all_os() const {
    std::vector<std::string> res;
    for (const auto& item : m_data) {
        if (std::find(res.begin(), res.end(), item.os) == res.end()) {
            res.push_back(item.os);
        }
    }
    return res;
}

std::vector<std::string> ImageDb::get_releases(const std::string& os) const {
    std::vector<std::string> res;
    for (const auto& item : m_data) {
        if (item.os == os && std::find(res.begin(), res.end(), item.release) == res.end()) {
            res.push_back(item.release);
        }
    }
    return res;
}

std::vector<std::string> ImageDb::get_archs(const std::string& os, const std::string& release) const {
    std::vector<std::string> res;
    for (const auto& item : m_data) {
        if (item.os == os && item.release == release
            && std::find(res.begin(), res.end(), item.arch) == res.end()) {
            res.push_back(item.arch);
        }
    }
    return res;
}

std::string ImageDb::get_image_path(const std::string& os,
                                    const std::string& release,
                                    const std::string& arch,
                                    const std::string& variant) const {
    for (const auto& item : m_data) {
        if (item.os == os && item.release == release
            && item.arch == arch && item.variant == variant) {
            return item.path;
        }
    }
    return "";
}
