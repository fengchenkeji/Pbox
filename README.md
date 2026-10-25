# Pbox

**无 Root 权限的 Android proot 容器管理器** — 基于 **Qt 6.12** / QML，参考 tiny_container 的图形化体验与 Pbox 的 proot 核心能力。Material3（谷歌设计语言）悬浮底部导航界面，由 GitHub Actions 使用 Android NDK 自动编译 APK。

## 功能特性

- **图形化容器管理**：一键下载、安装、启动、删除 Linux 发行版（Material3 界面）
- **自动架构识别**：arm64 / armhf / amd64 / riscv64 自动匹配 rootfs
- **多镜像源容错**：清华、南阳理工、阿里云、网易、LXC 官方自动切换
- **智能解压**：多压缩格式探测（zstd/xz/gzip），自动修复 `resolv.conf` 与 usrmerge 软链接
- **内置 proot 真实隔离运行**：proot 二进制随 APK 打包（assets），启动时 `proot --link2symlink -0 -r` 隔离容器；proot 不可用时自动降级系统 shell
- **PTY 伪终端**：内置 fork/setsid 伪终端，支持交互式 shell，终端输出实时显示
- **递归容器体积统计**：目录递归计算真实占用
- **Material3 界面**：悬浮底部导航（容器/安装/终端/日志/设置）、选中高亮、明暗双主题
- **Android 9 ~ 15+ 适配**：minSdk 28（Qt6.12 要求），targetSdk 35，arm64-v8a

## 支持的发行版

| 发行版 | 版本示例 |
|---|---|
| Ubuntu | noble (24.04), jammy (22.04), focal, mantic |
| Debian | trixie, bookworm, bullseye |
| Alpine | 3.20, 3.19, edge |
| Fedora | 40, 39, 41 |
| Arch Linux | latest |
| Kali | current |

## 项目结构

```
.
├── CMakeLists.txt              # Qt6.12 + Android NDK 交叉编译脚本
├── main.cpp                    # 入口，后台准备 proot
├── Main.qml                    # Material3 主界面（容器/安装/终端/日志/设置）
├── src/
│   ├── container_manager.cpp   # 下载/解压/修复/proot 命令生成
│   ├── pty_process.cpp         # 伪终端（fork/setsid/读写）
│   ├── terminal_bridge.cpp     # 终端会话桥接（proot 启动 + 降级）
│   └── pbox_paths.cpp          # 路径管理
├── android/
│   ├── AndroidManifest.xml     # 包名 com.pbox.app（SDK版本由Qt6 gradle控制, minSdk28）
│   └── assets/                 # proot 二进制（CI 自动下载打进 APK）
└── .github/workflows/
    └── android-apk.yml         # Qt6.12 + NDK 自动编译 APK
```

## 构建

APK 由 GitHub Actions 自动构建（见 `.github/workflows/android-apk.yml`），流程：
1. 安装 JDK 17，探测 runner 预装 Android SDK/NDK
2. 用 `jurplel/install-qt-action` 安装 **Qt 6.12.0 for Android**（`host=all_os` 源，自动配 host Qt 到 QT_ROOT_DIR/QT_HOST_PATH），并加装 `qttasktree` 模块
3. 从 Termux 仓库动态解析下载最新 proot 二进制打进 `android/assets/proot`
4. 用 NDK toolchain 交叉编译，`androiddeployqt` 打包出 APK（AGP 9）

### 本地构建（需 Qt 6.12 for Android + NDK 26+）
```bash
mkdir -p build && cd build
cmake .. \
  -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-28 \
  -DANDROID_SDK_ROOT=$ANDROID_SDK_ROOT \
  -DCMAKE_PREFIX_PATH=$QT_ANDROID_DIR \
  -DQT_HOST_PATH=$QT_HOST_DIR
cmake --build . --config Release
```

### 桌面调试（Linux 桌面 Qt6.12）
```bash
# 在项目根目录（qml 模块已注册 C++ 类型）
/path/to/Qt/6.12.0/gcc_64/bin/qml Main.qml
# 或
cmake -S . -B build-desk && cmake --build build-desk && ./build-desk/appPbox
```

## 打包说明

- proot 二进制来自 Termux 仓库（GPLv2 许可证），随 APK 打进 `assets/proot`
- 首次启动会把 proot 从 assets 复制到应用私有目录并 `chmod 755`
- 若 proot 缺失或启动失败，终端自动降级为系统 shell（仍可浏览容器文件）

## 许可证

GPLv3（proot 为 GPLv2，见 THIRD_PARTY_NOTICES）。
