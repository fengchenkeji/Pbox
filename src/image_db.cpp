#include "image_db.h"
#include "call_so.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

// 去除字符串首尾空格
static std::string trim(const std::string &s)
{
    auto start = s.begin();
    while (start != s.end() && std::isspace(*start))
        start++;

    auto end = s.end();
    do
    {
        end--;
    } while (std::distance(start, end) > 0 && std::isspace(*end));

    return std::string(start, end + 1);
}

static std::vector<std::string> split(const std::string& s, char delim)
{
    std::vector<std::string> res;
    std::stringstream ss(s);
    std::string tmp;
    while(std::getline(ss,tmp,delim)){
        res.push_back(tmp);
    }
    return res;
}

bool ImageDb::load(const std::string& filepath,bool debug)
{
    std::ifstream f(filepath);
    if(!f.is_open()){
        auto log = get_console_logger();
        SPDLOG_LOGGER_ERROR(log,"open {} failed",filepath);
        return false;
    }
    m_data.clear();
    std::string line;
    while(std::getline(f,line)){
        if(line.empty()) continue;
        auto kvList = split(line,',');
        ImageInfo info;
        for(auto& kv:kvList){
            auto pos = kv.find(':');
            if(pos==std::string::npos) continue;
            std::string k = trim(kv.substr(0,pos));
            std::string v = trim(kv.substr(pos+1));

            if(k=="OS") info.os=v;
            else if(k=="Release") info.release=v;
            else if(k=="Arch") info.arch=v;
            else if(k=="Variant") info.variant=v;
            else if(k=="Path") info.path=v;
        }
        // 去空格后再判断是否有效
        if(!info.os.empty() && !info.release.empty())
            m_data.push_back(info);
    }
    auto log = get_console_logger();
    DBG(log, debug, "load {} records", m_data.size());
    return true;
}

void ImageDb::get_all_os(std::vector<std::string>& out_os) const
{
    out_os.clear();
    for(const auto& info : m_data)
    {
        if(std::find(out_os.begin(), out_os.end(), info.os) == out_os.end())
        {
            out_os.push_back(info.os);
        }
    }
}
