

# Pbox

## 项目介绍

Pbox 是一款运行在 **Termux** 环境下的轻量 proot 容器管理工具，无需 Root 权限，即可快速创建、初始化并管理 proot 容器。

**Pbox 自带 proot 二进制**，不依赖系统安装的 proot 包，通过相对路径调用，完全进程隔离。

## 核心功能

1. **自动架构识别**：自动检测 Termux 用户空间架构（armhf/arm64/amd64），下载匹配的 rootfs 镜像
2. **多镜像源容错**：内置清华、南阳理工、LXC 官方多个镜像源，自动切换
3. **智能解压**：还原文件权限，自动补全 usrmerge 软链接，跳过 Android 不支持的设备文件
4. **容器管理**：install 下载安装，login 直接启动已安装容器，list 查看可用系统版本
5. **自带 proot**：proot 二进制打包在 deb 内，使用相对路径调用，不依赖系统 proot
6. **版本查询**：`pbox proot-version` 查看内置 proot 版本
7. **进程隔离**：fork+exec 调用 proot，崩溃不影响主程序

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

# 查看内置 proot 版本号
pbox proot-version

# 调试模式
pbox --run_type=debug install ubuntu:22.04

# 查看帮助
pbox -h
```

## 工作原理

Pbox 采用 **fork+exec** 方式调用自带的 proot：

```
pbox 主程序
  └─ libinitialization.so
       └─ fork()
            └─ execv(bin/proot)  ← 自带 proot 二进制，相对路径
```

- proot 运行在独立子进程中，ptrace、信号处理完全隔离
- proot 二进制位于 `opt/Pbox/bin/proot`
- 主程序通过可执行文件所在目录定位 `bin/proot`，使用相对路径

## 安装（Termux）

下载 CI 构建的 `.deb` 包后直接安装：

```bash
# arm64 (大多数手机)
dpkg -i pbox-aarch64.deb

# arm32 (旧设备)
dpkg -i pbox-arm.deb
```

安装后直接运行 `pbox` 即可。

## 编译方式（termux-packages）

Pbox 作为 termux-packages 包构建：

```bash
git clone https://github.com/termux/termux-packages.git
cd termux-packages

# 将 Pbox 的 build.sh 和源码放入 packages/pbox/
# 然后构建
./scripts/run-docker.sh ./build-package.sh -a aarch64 pbox
./scripts/run-docker.sh ./build-package.sh -a arm pbox
```

## 依赖

- **libspdlog**：日志组件（Termux 系统包）
- **libcurl**：HTTP 下载镜像（Termux 系统包）
- **proot**：**自带**，打包在 deb 内（`opt/Pbox/bin/proot`）
- **libtalloc**：**自带**，打包在 deb 内（`opt/Pbox/lib/libtalloc.so`）

## GitHub Actions

CI 使用 termux-packages Docker 环境构建：

| Job | 架构 | 产物 |
|-----|------|------|
| build-aarch64 | arm64 | `pbox-aarch64.deb` |
| build-arm | arm32 | `pbox-arm.deb` |

每个 deb 包内含：
- `opt/Pbox/pbox` — 主程序
- `opt/Pbox/bin/proot` — 自带 proot 二进制
- `opt/Pbox/lib/` — 共享库（libinitialization.so、libtalloc.so 等）
- `bin/pbox` — 启动脚本

## 项目结构

```
Pbox/
├── include/          # 头文件
├── src/              # 源代码
├── bin/              # 预编译 proot（可选，CI 自动打包）
├── scripts/          # 构建/测试脚本
├── .github/workflows/
│   └── ci.yml        # termux-packages CI 构建
├── build.sh          # termux-packages 包脚本
├── CMakeLists.txt    # CMake 构建配置
└── README.md
```

## 许可证

Pbox 基于 **GPL-3.0-or-later** 许可证发布。

proot 为 GPL-2.0-or-later，完整版权信息见 [THIRD_PARTY_NOTICES.md](./THIRD_PARTY_NOTICES.md)。
