#!/bin/bash
# build-proot-linux.sh - 在 Linux x86_64 上编译 termux/proot 为可执行文件
#
# 用途：为 Pbox 的 Linux 测试构建生成 bin/x86_64/proot 可执行文件
# 原理：标准编译 proot 为 PIE 可执行文件，Pbox 通过 fork+exec 调用
#
# 用法：./scripts/build-proot-linux.sh [输出目录]
# 默认输出到 lib/x86_64/proot
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
OUTPUT_DIR="${1:-${PROJECT_ROOT}/lib/x86_64}"
PROOT_VERSION="5.1.107.92"
WORK_DIR="${PROJECT_ROOT}/build/proot-build"

echo "============================================"
echo "  Building proot executable for Linux x86_64"
echo "  proot version: ${PROOT_VERSION}"
echo "  output: ${OUTPUT_DIR}/proot"
echo "============================================"

# 检查依赖
if ! command -v gcc &>/dev/null; then
    echo "[ERROR] gcc not found"
    exit 1
fi
if ! pkg-config --exists talloc 2>/dev/null && [ ! -f /usr/include/talloc.h ]; then
    echo "[ERROR] libtalloc-dev not found (install: apt install libtalloc-dev)"
    exit 1
fi

# 克隆源码
mkdir -p "$(dirname "$WORK_DIR")"
if [ ! -d "${WORK_DIR}/proot" ]; then
    echo "[INFO] Cloning termux/proot..."
    git clone --depth 1 --branch "v${PROOT_VERSION}" \
        https://github.com/termux/proot.git "${WORK_DIR}/proot" 2>/dev/null || \
    git clone --depth 1 \
        https://github.com/termux/proot.git "${WORK_DIR}/proot"
fi

cd "${WORK_DIR}/proot/src"

# 清理旧构建
make clean 2>/dev/null || true

# 编译为标准可执行文件（PIE）
echo "[INFO] Compiling proot as executable..."
make -j"$(nproc)" \
    CC=gcc \
    CFLAGS="-O2 -DARG_MAX=131072 -DVERSION=\"${PROOT_VERSION}\"" \
    LDFLAGS="-Wl,-z,noexecstack -ltalloc" \
    V=0

# 验证产物
if [ ! -f proot ]; then
    echo "[ERROR] proot binary not found after build"
    exit 1
fi

# 复制到输出目录
mkdir -p "${OUTPUT_DIR}"
cp -f proot "${OUTPUT_DIR}/proot"
chmod 755 "${OUTPUT_DIR}/proot"

echo ""
echo "[OK] proot built successfully:"
ls -la "${OUTPUT_DIR}/proot"
file "${OUTPUT_DIR}/proot"

# 验证版本
echo ""
echo "[INFO] Version check:"
"${OUTPUT_DIR}/proot" --version
