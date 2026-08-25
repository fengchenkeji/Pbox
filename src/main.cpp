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
#include "Cli_menu.h"
#include "image_db.h"
#include "write_log.h"
#include <string>
#include <fstream>
#include <sstream>
#include <ctime>
#include <cerrno>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <filesystem>

namespace fs = std::filesystem;
using json = nlohmann::json;
std::shared_ptr<spdlog::logger> get_console_logger();

// 从网络下载文件到本地
static bool download_file(const std::string& url, const std::string& out_path, bool debug)
{
    auto logger = get_console_logger();
    CURL* curl = curl_easy_init();
    if (!curl)
    {
        DBG(logger, debug, "curl 初始化失败");
        return false;
    }

    fs::path out_p(out_path);
    fs::create_directories(out_p.parent_path());

    FILE* fp = fopen(out_path.c_str(), "wb");
    if (!fp)
    {
        DBG(logger, debug, "无法创建缓存文件:{} errno:{}", out_path, errno);
        curl_easy_cleanup(curl);
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Android; Termux) curl/8.21.0");

    CURLcode res = curl_easy_perform(curl);
    fclose(fp);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
    {
        DBG(logger, debug, "下载失败 curl code:{} msg:{}", (int)res, curl_easy_strerror(res));
        if (fs::exists(out_path))
            fs::remove(out_path);
        return false;
    }

    // 校验是否为合法JSON，非JSON直接删除脏文件
    std::ifstream test(out_path);
    json test_json;
    try {
        test >> test_json;
    } catch (...) {
        DBG(logger, debug, "服务器返回非JSON内容，删除脏缓存文件");
        if (fs::exists(out_path))
            fs::remove(out_path);
        return false;
    }

    DBG(logger, debug, "镜像元数据下载完成:{}", out_path);
    return true;
}

// 获取本地日期字符串 YYYY-MM-DD
static std::string get_today_str()
{
    time_t now = time(nullptr);
    tm t = *localtime(&now);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d", &t);
    return std::string(buf);
}

// 读取缓存记录的日期
static std::string read_cache_date(const std::string& date_file, bool debug)
{
    auto logger = get_console_logger();
    std::ifstream f(date_file);
    if (!f.is_open())
    {
        DBG(logger, debug, "无缓存日期文件，需重新下载");
        return "";
    }
    std::string date;
    std::getline(f, date);
    f.close();
    return date;
}

// 写入今日日期到缓存标记文件
static void write_cache_date(const std::string& date_file, const std::string& today, bool debug)
{
    auto logger = get_console_logger();
    fs::path dp(date_file);
    fs::create_directories(dp.parent_path());
    std::ofstream f(date_file);
    if (!f.is_open())
    {
        DBG(logger, debug, "无法写入缓存日期标记");
        return;
    }
    f << today;
    f.close();
}

int main(int argc, char* argv[])
{
    // 初始化日志
    init_error_file_log("./logs");
    auto logger = get_console_logger();

    // curl 全局初始化
    curl_global_init(CURL_GLOBAL_ALL);

    // 获取程序运行目录
    std::string full_path = argv[0];
    size_t slash = full_path.rfind('/');
    std::string exe_dir;
    if (slash != std::string::npos)
        exe_dir = full_path.substr(0, slash);
    else
        exe_dir = ".";

    // 缓存文件路径定义
    std::string cache_json = exe_dir + "/res/images_cache.json";
    std::string date_mark = exe_dir + "/res/.cache_date";
    // 双源：清华镜像优先，失败切官方
    const std::string meta_url = "https://mirrors.tuna.tsinghua.edu.cn/lxc-images/meta/simplestreams/v1/images.json";
    const std::string fallback_url = "https://images.linuxcontainers.org/meta/simplestreams/v1/images.json";
    const std::string nyist_url = "https://mirror.nyist.edu.cn/lxc-images/meta/simplestreams/v1/images.json";

    // 解析命令行参数
    CliParseResult cli_arg = parse_cli(argc, argv);
    ImageDb db;

    std::string today = get_today_str();
    std::string cache_day = read_cache_date(date_mark, cli_arg.debug);
    bool need_download = (cache_day.empty() || cache_day != today);

    if (need_download)
    {
        DBG(logger, cli_arg.debug, "缓存过期/不存在，尝试下载元数据");
        bool ok = download_file(nyist_url, cache_json, cli_arg.debug);
        if (!ok)
        {
            DBG(logger, cli_arg.debug, "下载无效，切换清华园重试");
            ok = download_file(meta_url, cache_json, cli_arg.debug);
        }
        if (!ok)
        {
            DBG(logger, cli_arg.debug, "清华源下载无效，切换官方源重试");
            ok = download_file(fallback_url, cache_json, cli_arg.debug);
        }

        if (ok)
        {
            write_cache_date(date_mark, today, cli_arg.debug);
        }
        else
        {
            spdlog::warn("镜像元数据下载失败，尝试读取历史有效缓存");
            if (fs::exists(date_mark))
                fs::remove(date_mark);
        }
    }
    else
    {
        DBG(logger, cli_arg.debug, "今日缓存有效，直接读取本地文件");
    }

    bool load_ok = db.load(cache_json, cli_arg.debug);
    
    // 新增：本地缓存损坏时，强制重新下载一次
    if (!load_ok && !need_download)
    {
        DBG(logger, cli_arg.debug, "本地缓存损坏，强制重新下载元数据");
        if (fs::exists(cache_json)) fs::remove(cache_json);
        if (fs::exists(date_mark)) fs::remove(date_mark);

        bool ok = download_file(meta_url, cache_json, cli_arg.debug);
        if (!ok) ok = download_file(fallback_url, cache_json, cli_arg.debug);
        
        if (ok) write_cache_date(date_mark, today, cli_arg.debug);
        load_ok = db.load(cache_json, cli_arg.debug);
    }

    if (!load_ok)
    {
        spdlog::critical("无可用镜像缓存，程序无法继续");
        curl_global_cleanup();
        return 1;
    }

    int ret = execute_command(cli_arg, db, exe_dir);
    curl_global_cleanup();
    return ret;
}
