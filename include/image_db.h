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
public:
    bool load(const std::string& filepath, bool debug);
    // 必须和cpp实现一模一样：参数+const修饰
    void get_all_os(std::vector<std::string>& out_os) const;

    std::vector<std::string> get_releases(const std::string& os) const;
    std::vector<std::string> get_archs(const std::string& os, const std::string& release) const;
    std::string get_image_path(const std::string& os, const std::string& release, const std::string& arch, const std::string& variant="default") const;
private:
    std::vector<ImageInfo> m_data;
};
