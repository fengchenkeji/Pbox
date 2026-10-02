# Pbox

**无 Root 权限的 Android proot 容器管理器** — 基于 Qt6 / QML，参考 [tiny_container](https://github.com/Cateners/tiny_container) 的图形化体验与 Pbox 的 proot 核心能力。

## 功能特性

- **图形化容器管理**：一键下载、安装、启动、删除 Linux 发行版
- **自动架构识别**：arm64 / armhf / amd64 自动匹配 rootfs
- **多镜像源容错**：清华、南阳理工、阿里云、网易、LXC 官方自动切换
- **智能解压**：通过 `proot --link2symlink tar` 还原权限，自动修复 `resolv.conf` 与 usrmerge 软链接
- **内置 proot**：从 Termux 仓库自动下载 proot 二进制，打包进 APK，无需 Root
- **PTY 终端**：内置伪终端，支持交互式 shell
- **Android 8 ~ 15+ 适配**：minSdk 26，targetSdk 35

## 支持的发行版

| 发行版 | 版本示例 |
|--------|----------|
| Ubuntu | noble (24.04), jammy (22.04), focal |
| Debian | trixie, bookworm, bullseye |
| Alpine | 3.20, 3.19, edge |
| Fedora | 40, 39 |
| Arch Linux | latest |
| Kali | current |

## 项目结构

```
android-app/
├── CMakeLists.txt          # Qt6 构建脚本
├── android/                # Android 工程
│   ├── AndroidManifest.xml
│   ├── build.gradle
│   └── res/
├── qml/
│   └── main.qml            # 主界面（容器列表/安装/终端）
└── src/
    ├── main.cpp             # 入口 + TerminalBridge
    ├── container_manager.h/cpp   # 下载/解压/proot启动
    ├── pbox_paths.h/cpp     # 路径管理
    └── pty_process.h/cpp    # 伪终端
```

## 构建

项目通过 GitHub Actions 自动构建 APK（见 `.github/workflows/android-apk.yml`）。

本地构建需要：
- Qt 6.5.2+ for Android (arm64_v8a)
- Android NDK 25+
- CMake 3.16+

```bash
mkdir build && cd build
cmake .. \
  -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-26 \
  -DCMAKE_PREFIX_PATH=$QT_ANDROID_DIR \
  -DQT_HOST_PATH=$QT_HOST_DIR
make -j$(nproc)
cd android-build && ./gradlew assembleRelease
```

## 许可证

GPLv3
