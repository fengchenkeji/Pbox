#!/bin/bash
# test-proot-call.sh - 验证 Pbox 通过 fork+exec 调用 proot 可执行文件
#
# 测试内容：
#   1. 检查 proot 可执行文件存在且可执行
#   2. 执行 proot --version 验证版本输出
#   3. 检查 Pbox 主程序可执行
#   4. 通过 Pbox 调用 proot-version 命令验证完整链路
#
# 用法：./scripts/test-proot-call.sh [构建目录]
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$(cd "${1:-${PROJECT_ROOT}/build}" 2>/dev/null && pwd || echo "${PROJECT_ROOT}/build")"

# 查找 proot 可执行文件（优先 build/bin，其次 lib/x86_64）
PROOT_BIN=""
if [ -f "${BUILD_DIR}/bin/proot" ]; then
    PROOT_BIN="${BUILD_DIR}/bin/proot"
elif [ -f "${PROJECT_ROOT}/lib/x86_64/proot" ]; then
    PROOT_BIN="${PROJECT_ROOT}/lib/x86_64/proot"
elif command -v proot &>/dev/null; then
    PROOT_BIN="$(command -v proot)"
fi

echo "============================================"
echo "  Testing Pbox -> proot executable call chain"
echo "  build dir:   ${BUILD_DIR}"
echo "  proot bin:   ${PROOT_BIN:-<not found>}"
echo "============================================"

# 检查 proot 可执行文件
if [ -z "$PROOT_BIN" ]; then
    echo "[WARN] proot executable not found, skipping exec test"
    exit 0
fi
echo "[OK] proot executable exists"
file "$PROOT_BIN"

# 检查 Pbox 主程序
if [ ! -f "${BUILD_DIR}/main" ]; then
    echo "[FAIL] Pbox main not found: ${BUILD_DIR}/main"
    exit 1
fi
echo "[OK] Pbox main executable exists"

# 测试 proot --version（直接执行）
echo ""
echo "=== Direct proot --version ==="
"$PROOT_BIN" --version

# 测试 Pbox proot-version 命令
echo ""
echo "=== Pbox proot-version ==="
cd "${BUILD_DIR}"
./main proot-version

echo ""
echo "[OK] All tests passed!"
