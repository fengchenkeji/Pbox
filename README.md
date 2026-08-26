

# Pbox

## 项目介绍

Pbox 是一款运行在 Termux 环境下的轻量 proot 工具箱，无需 Android 设备 Root 权限，即可快速创建、初始化并管理 proot 容器，轻松搭建隔离的 Linux 运行环境。

## 核心功能

1. **自动架构识别**：自动检测 Termux 用户空间架构（armhf/arm64/amd64），下载匹配的 rootfs 镜像
2. **多镜像源容错**：内置清华、南阳理工、网易、阿里云、LXC 官方多个镜像源，自动切换
3. **智能解压**：还原文件权限，自动补全 usrmerge 软链接，跳过 Android 不支持的设备文件/FIFO
4. **容器管理**：install 下载安装，login 直接启动已安装容器，list 查看可用系统版本
5. **完善日志**：控制台打印全部日志，错误日志持久化存储

## 命令用法

```bash
# 列出所有可用系统
pbox list

# 列出指定系统的所有版本
pbox list ubuntu

# 安装容器（自动匹配架构，下载解压后直接启动）
pbox install ubuntu:22.04

# 启动已安装的容器
pbox login ubuntu:22.04

# 调试模式（输出详细日志）
pbox --run_type=debug install ubuntu:22.04

# 查看帮助
pbox -h
```

## 编译方式

### 方式一：termux-packages 构建（推荐）

```bash
git clone https://github.com/termux/termux-packages.git
cd termux-packages
./build-package.sh -a aarch64 Pbox
```

### 方式二：Termux 内手动编译

```bash
# 安装依赖
pkg install cmake libspdlog libcurl libarchive clang

# 编译
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## 依赖

- **libspdlog**：日志组件
- **libcurl**：HTTP 下载镜像元数据
- **libarchive**：解压 tar.xz rootfs 压缩包
- **libproot.so**：proot 核心动态库（arm32 架构预编译，位于 lib/ 目录）

## 项目结构

```
Pbox/
├── include/          # 头文件
│   ├── Cli_menu.h        # 命令行解析与业务逻辑
│   ├── call_so.h         # 动态库加载工具
│   ├── container_config.h # 容器配置管理
│   ├── download_rootfs.hpp # rootfs 下载与解压
│   ├── image_db.h        # 镜像数据库
│   ├── initialization.h  # proot 启动器
│   └── write_log.h       # 日志系统
├── src/              # 源代码
│   ├── Cli_menu.cpp      # 命令行解析与业务逻辑实现
│   ├── call_so.cpp       # 动态库加载工具实现
│   ├── container_config.cpp # 容器配置管理实现
│   ├── download_rootfs.cpp # rootfs 下载与解压实现
│   ├── image_db.cpp      # 镜像数据库实现（JSON 解析）
│   ├── initialization.cpp # proot 启动器实现
│   ├── main.cpp          # 程序入口，镜像元数据缓存
│   └── write_log.cpp     # 日志系统实现
├── lib/              # 预编译动态库
│   └── libproot.so       # proot 核心库（arm32）
├── res/              # 资源文件
├── build.sh          # termux-packages 构建脚本
├── CMakeLists.txt    # CMake 构建配置
└── README.md
```

## 技术说明

- rootfs 解压时跳过 tar 内的符号链接、设备文件、FIFO，避免 Android 权限崩溃
- 解压后自动补全 usrmerge 根目录软链接（/bin /lib /sbin）和动态链接器
- armhf 架构下 ld-linux-armhf.so.3 为实体文件，不创建覆盖软链接
- proot 启动参数：--link2symlink --kill-on-exit --sysvipc -0，绑定 /dev /proc /sys

## 许可证

Pbox 基于 **GPL-3.0-or-later** 许可证发布，详情请参阅 `COPYING` 文件。

### 第三方组件

- **libproot.so**：预编译版本（非 Termux 官方仓库提供），GPL-2.0-or-later
- **spdlog**、**libcurl**、**libarchive**、**nlohmann-json**：依赖 Termux 系统包

完整的第三方版权信息请参阅 [THIRD_PARTY_NOTICES.md](./THIRD_PARTY_NOTICES.md)。