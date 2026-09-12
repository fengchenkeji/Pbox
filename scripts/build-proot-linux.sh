#!/bin/bash
# build-proot-linux.sh - 在 Linux x86_64 上编译 termux/proot 为 libproot.so（共享对象）
#
# 用途：为 Pbox 的 Linux 测试构建生成 lib/x86_64/libproot.so
# 原理：用 -shared 链接 proot 的 .o 文件，使其成为 ET_DYN 共享对象，
#       main 符号保留在符号表中，Pbox 通过 dlopen+dlsym 调用。
#
# 用法：./scripts/build-proot-linux.sh [输出目录]
# 默认输出到 lib/x86_64/libproot.so
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
OUTPUT_DIR="${1:-${PROJECT_ROOT}/lib/x86_64}"
PROOT_VERSION="5.1.107.92"
WORK_DIR="${PROJECT_ROOT}/build/proot-build"

echo "============================================"
echo "  Building libproot.so for Linux x86_64"
echo "  proot version: ${PROOT_VERSION}"
echo "  output: ${OUTPUT_DIR}/libproot.so"
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

# 编译为共享对象
# 关键：
#   -fPIC: 位置无关代码（共享库必须）
#   -DARG_MAX=131072: 兼容 Linux 大参数
#   -DVERSION: proot 版本号
#   LDFLAGS=-shared: 生成 ET_DYN 共享对象（不使用 -nostdlib，否则 __dso_handle 未定义）
#   -ltalloc: 链接 talloc 内存库
echo "[INFO] Compiling proot as shared object..."
make -j"$(nproc)" \
    CC=gcc \
    CFLAGS="-O2 -fPIC -DARG_MAX=131072 -DVERSION=\"${PROOT_VERSION}\"" \
    LDFLAGS="-shared -lc -Wl,-z,noexecstack -ltalloc" \
    V=0

# 验证产物
if [ ! -f proot ]; then
    echo "[ERROR] proot binary not found after build"
    exit 1
fi

if ! file proot | grep -q "shared object"; then
    echo "[ERROR] proot is not a shared object:"
    file proot
    exit 1
fi

# 复制到输出目录
mkdir -p "${OUTPUT_DIR}"
cp -f proot "${OUTPUT_DIR}/libproot.so"
chmod 755 "${OUTPUT_DIR}/libproot.so"

echo ""
echo "[OK] libproot.so built successfully:"
ls -la "${OUTPUT_DIR}/libproot.so"
file "${OUTPUT_DIR}/libproot.so"

# 验证 main 符号存在
echo ""
echo "[INFO] Symbol check:"
if nm -D "${OUTPUT_DIR}/libproot.so" 2>/dev/null | grep -q " T main"; then
    echo "  main in .dynsym: YES (dlsym can find it directly)"
elif nm "${OUTPUT_DIR}/libproot.so" 2>/dev/null | grep -q " T main"; then
    echo "  main in .symtab: YES (ELF fallback needed)"
else
    echo "  WARNING: main symbol not found"
fi
