#!/usr/bin/env bash
# ============================================================
# Pbox - Linux arm64 构建脚本
# 目标：在 arm64 Linux（如树莓派4/5、RK3588、aarch64云主机、Termux proot）上原生编译
# 前置：Qt 6.12.0 for Linux arm64 已安装（见下方 Qt 安装步骤）
# ============================================================
set -e

# ---------- 配置（按需修改） ----------
QT_ROOT="${QT_ROOT:-$HOME/qt/6.12.0/gcc_64}"   # Qt 安装目录（arm64 版本）
BUILD_DIR="${BUILD_DIR:-build-linux-arm64}"
CMAKE_FLAGS=()

# ---------- 0. 检查架构 ----------
echo "当前架构: $(uname -m)"
[ "$(uname -m)" = "aarch64" ] || echo "⚠️  警告: 非 arm64 架构($(uname -m))，请确认在目标机/交叉环境运行"

# ---------- 1. 检查 Qt ----------
if [ ! -f "$QT_ROOT/lib/cmake/Qt6/Qt6Config.cmake" ]; then
    echo "❌ 未找到 Qt 6.12.0: $QT_ROOT"
    echo "请先安装 Qt 6.12.0 for Linux arm64，方法见脚本底部说明。"
    exit 1
fi
echo "✅ Qt 6.12.0: $QT_ROOT"

# ---------- 2. 依赖库检查（Qt 运行所需） ----------
echo "--- 检查系统依赖库 ---"
MISSING=()
for lib in libGL.so.1 libfontconfig.so.1 libdbus-1.so.3; do
    if ! ldconfig -p 2>/dev/null | grep -q "$lib"; then
        MISSING+=("$lib")
    fi
done
if [ ${#MISSING[@]} -gt 0 ]; then
    echo "❌ 缺少系统库: ${MISSING[*]}"
    echo "   Debian/Ubuntu:  sudo apt install -y libgl1 libfontconfig1 libdbus-1-3 libxkbcommon0 libxcb-cursor0"
    echo "   Fedora:         sudo dnf install -y mesa-libGL fontconfig dbus-libs libxkbcommon xcb-util-cursor"
    exit 1
fi
echo "✅ 系统依赖库就绪"

# ---------- 3. CMake 版本检查 ----------
CMAKE_VER=$(cmake --version | head -1 | awk '{print $3}')
echo "CMake 版本: $CMAKE_VER"
if [ "$(printf '%s\n' "$CMAKE_VER" "3.25" | sort -V | head -1)" = "$CMAKE_VER" ] && [ "$CMAKE_VER" != "3.25" ]; then
    echo "⚠️  CMake 低于 3.25（Qt 6.12 要求），添加兼容参数"
    CMAKE_FLAGS+=("-DQT_FORCE_MIN_CMAKE_VERSION_FOR_USING_QT=3.22")
fi

# ---------- 4. 配置 ----------
echo "--- 配置 CMake ---"
cmake -S . -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$QT_ROOT" \
    "${CMAKE_FLAGS[@]}"

# ---------- 5. 编译 ----------
echo "--- 编译（$(nproc) 核） ---"
cmake --build "$BUILD_DIR" --config Release -j"$(nproc)"

# ---------- 6. 运行 ----------
echo ""
echo "✅ 构建完成: $BUILD_DIR/appPbox"
echo "运行:"
echo "   export LD_LIBRARY_PATH=$QT_ROOT/lib:$QT_ROOT/plugins"
echo "   $BUILD_DIR/appPbox"
echo ""
echo "（无显示器环境可加 -platform offscreen 验证启动）"

# ============================================================
# 附：Qt 6.12.0 for Linux arm64 安装方法
# ============================================================
# 方法A：在 arm64 目标机上用 aqtinstall（需 python3 + pip）
#   pip install aqtinstall
#   python3 -m aqt install-qt linux desktop 6.12.0 linux_gcc_arm64 --outputdir ~/qt
#
# 方法B：交叉编译（在 x86 主机上构建 arm64 程序）
#   安装交叉工具链:  sudo apt install -y g++-aarch64-linux-gnu
#   然后手动配置 cmake 用 aarch64-linux-gnu-g++ + arm64 版 Qt。
#   注意：Qt 本身也必须用 aarch64 版本，交叉编译较复杂，建议优先方法A原生编译。
#
# 方法C：Termux（Android 上的 Linux 环境）
#   在 Termux proot 的 aarch64 Ubuntu/Debian 里，走方法A 即可。
# ============================================================
