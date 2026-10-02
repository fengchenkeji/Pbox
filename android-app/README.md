# Pbox Android

将 Pbox 从 Termux CLI 工具改造为独立 Android App，使用 Qt Quick 渲染界面。

## 架构

```
Pbox Android App
├── Qt Quick UI (QML)
│   ├── 容器列表页面
│   ├── 安装页面（选择OS/版本，下载进度）
│   └── 终端页面（伪终端交互）
├── C++ 核心层
│   ├── PboxPaths     - Android私有目录/nativeLibDir路径管理
│   ├── PtyProcess    - 伪终端(openpt)进程管理
│   └── ContainerManager - 下载/解压/启动proot
└── Native层
    └── libpbox_proot.so - 从Termux deb提取的proot二进制
```

## 与Termux版本的区别

| 项目 | Termux版 | Android独立版 |
|------|---------|--------------|
| 运行环境 | 需要安装Termux | 独立APK，不依赖Termux |
| UI | 命令行 | Qt Quick图形界面 |
| proot位置 | $PREFIX/opt/Pbox/bin/proot | APK lib目录（W^X豁免） |
| rootfs存储 | $PREFIX/opt/Pbox/proot/container | /data/data/com.pbox.app/files/rootfs |
| 终端交互 | 系统终端 | 内置伪终端(QML) |

## 鸿蒙兼容性

| 系统版本 | 兼容性 | 说明 |
|---------|--------|------|
| HarmonyOS NEXT (5.0+) | ❌ 不兼容 | 纯血鸿蒙不支持APK/Linux ptrace |
| HarmonyOS 4.x (AOSP) | ⚠️ 需真机测试 | 华为SELinux可能拦截ptrace loader |
| Android 7.0+ | ✅ 支持 | minSdk 24 |

## 构建

GitHub Action 自动构建：推送到main分支即触发。

本地构建要求：
- Qt 6.5+ (Android组件: arm64-v8a)
- Android NDK 26+
- CMake 3.16+

## 目录结构

```
android-app/
├── CMakeLists.txt      # Qt构建配置
├── src/
│   ├── main.cpp        # Qt入口
│   ├── pbox_paths.h/cpp    # 路径管理
│   ├── pty_process.h/cpp  # 伪终端
│   └── container_manager.h/cpp  # 容器管理
├── qml/
│   └── main.qml        # UI界面
└── android/
    ├── AndroidManifest.xml
    └── res/            # 资源文件
```
