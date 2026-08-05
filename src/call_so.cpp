#include <iostream>
#include <dlfcn.h>
#include <cstring>
#include <spdlog/sinks/stdout_color_sinks.h>
#include "../include/call_so.h"

// 统一日志入口：完全依赖 spdlog 全局注册表做唯一性判断
std::shared_ptr<spdlog::logger> get_console_logger() {
    // 先从全局注册表查找，已存在直接复用
    auto logger = spdlog::get("console");
    if (logger) {
        return logger;
    }
    // 不存在才创建，工厂函数会自动注册，无需手动 register
    try {
        logger = spdlog::stdout_color_mt("console");
        return logger;
    } catch (const spdlog::spdlog_ex& e) {
        std::cerr << "[WARN] Logger init failed: " << e.what() << std::endl;
        return nullptr;
    }
}

bool is_debug(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--run_type=debug") == 0)
            return true;
    }
    return false;
}

void call_so(const char* so_path, const char* so_function, bool debug) {
    auto logger = get_console_logger();

    if (debug) {
        DBG(logger, "Loading SO: {}", so_path);
    }

    void* handle = dlopen(so_path, RTLD_LAZY);
    if (!handle) {
        DBG(logger, "Cannot open file: {}, error: {}", so_path, dlerror());
        return;
    }

    dlerror();
    void* func = dlsym(handle, so_function);
    if (!func) {
        DBG(logger, "Cannot find function: {} from {}, error: {}", 
            so_function, so_path, dlerror());
        dlclose(handle);
        return;
    }

    // 匹配 hello 函数真实签名 int()，避免未定义行为
    using func_t = int(*)();
    int ret = reinterpret_cast<func_t>(func)();
    
    if (debug) {
        DBG(logger, "Function {} executed, return code: {}", so_function, ret);
    }

    dlclose(handle);
}
