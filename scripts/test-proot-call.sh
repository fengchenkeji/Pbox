#!/bin/bash
# test-proot-call.sh - 验证 Pbox 调用 libproot.so 的完整链路
#
# 测试内容：
#   1. 编译 test_proot_so.c
#   2. dlopen libproot.so
#   3. dlsym "main"
#   4. 调用 proot main(--version) 和 main(--help)
#
# 用法：./scripts/test-proot-call.sh [构建目录]
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$(cd "${1:-${PROJECT_ROOT}/build}" 2>/dev/null && pwd || echo "${PROJECT_ROOT}/build")"
SO_PATH="${BUILD_DIR}/lib/libproot.so"
TEST_SRC="${BUILD_DIR}/test_proot_so.c"

echo "============================================"
echo "  Testing Pbox -> libproot.so call chain"
echo "  build dir: ${BUILD_DIR}"
echo "  so path:   ${SO_PATH}"
echo "============================================"

# 检查 libproot.so
if [ ! -f "${SO_PATH}" ]; then
    echo "[FAIL] libproot.so not found: ${SO_PATH}"
    exit 1
fi
echo "[OK] libproot.so exists"
file "${SO_PATH}"

# 检查 main 可执行文件
if [ ! -f "${BUILD_DIR}/main" ]; then
    echo "[FAIL] Pbox main not found: ${BUILD_DIR}/main"
    exit 1
fi
echo "[OK] Pbox main executable exists"

# 写入测试程序（如果不存在）
if [ ! -f "${TEST_SRC}" ]; then
    cat > "${TEST_SRC}" << 'CEOF'
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

typedef int (*ProotMain)(int argc, char **argv);

static int run_proot_main(ProotMain fn, const char *arg)
{
    pid_t pid = fork();
    if (pid == 0) {
        char *argv[] = { (char *)"proot", (char *)arg, NULL };
        _exit(fn(2, argv));
    }
    int status = 0;
    waitpid(pid, &status, 0);
    int rc = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    fprintf(stderr, "[TEST]   proot %-9s -> exit code = %d\n", arg, rc);
    return rc;
}

int main(int argc, char **argv)
{
    const char *so_path = (argc > 1) ? argv[1] : "./lib/libproot.so";

    fprintf(stderr, "[TEST] 1) dlopen(%s, RTLD_NOW|RTLD_GLOBAL) ...\n", so_path);
    void *handle = dlopen(so_path, RTLD_NOW | RTLD_GLOBAL);
    if (!handle) {
        fprintf(stderr, "[TEST] FAIL dlopen: %s\n", dlerror());
        return 1;
    }
    fprintf(stderr, "[TEST] OK   dlopen handle=%p\n", handle);

    fprintf(stderr, "[TEST] 2) dlsym(handle, \"main\") ...\n");
    dlerror();
    ProotMain proot_main = (ProotMain)dlsym(handle, "main");
    const char *e = dlerror();
    if (e != NULL || !proot_main) {
        fprintf(stderr, "[TEST] FAIL dlsym main: %s\n", e ? e : "(null)");
        dlclose(handle);
        return 2;
    }
    fprintf(stderr, "[TEST] OK   main @ %p\n", (void *)proot_main);

    fprintf(stderr, "----- proot output -----\n");
    int rc1 = run_proot_main(proot_main, "--version");
    int rc2 = run_proot_main(proot_main, "--help");

    dlclose(handle);

    if (rc1 == 0 && rc2 == 0) {
        fprintf(stderr, "[TEST] PASS: dlopen + dlsym(main) + main() call chain OK\n");
        return 0;
    }
    fprintf(stderr, "[TEST] FAIL: proot returned non-zero (version=%d help=%d)\n", rc1, rc2);
    return 3;
}
CEOF
fi

# 编译测试程序
echo "[INFO] Compiling test program..."
gcc -O2 -o "${BUILD_DIR}/test_proot_so" "${TEST_SRC}" -ldl

# 运行测试
echo ""
echo "[INFO] Running test..."
LD_LIBRARY_PATH="${BUILD_DIR}/lib:${LD_LIBRARY_PATH}" \
    "${BUILD_DIR}/test_proot_so" "${SO_PATH}"

echo ""
echo "[OK] All tests passed!"
