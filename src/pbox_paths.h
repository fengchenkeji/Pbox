// pbox_paths.h - 路径管理
#pragma once
#include <QString>

namespace PboxPaths {

// 应用私有文件目录（Android: /data/user/0/<pkg>/files；桌面: ~/.local/share/Pbox）
QString appFilesDir();

// 缓存目录（下载的 rootfs 压缩包、临时文件）
QString cacheDir();

// 日志目录
QString logDir();

// 容器 rootfs 根目录
QString rootfsRoot();

// 某个容器 tag 的 rootfs 目录
QString rootfsDir(const QString& tag);

// proot 二进制路径（打包在 assets 中，首运行复制到私有目录）
QString prootBin();

// proot 二进制在 assets 中的资源路径（qrc:/ 或 android assets:/）
QString prootAssetPath();

// 确保私有目录结构存在
void ensureDirs();

// 测试/调试用：覆盖根目录
void setRootOverrideForTests(const QString& path);

} // namespace PboxPaths
