#include "call_so.h"
#include <dlfcn.h>
#include <cstring>
#include <iostream>
#include <spdlog/sinks/stdout_color_sinks.h>

std::shared_ptr<spdlog::logger> get_console_logger()
{
    auto logger = spdlog::get("console");
    if(logger) return logger;
    try{
        logger = spdlog::stdout_color_mt("console");
    }catch(...){
        std::cerr << "create logger failed\n";
    }
    return logger;
}

bool is_debug(int argc, char* argv[])
{
    for(int i=1;i<argc;i++){
        if(strcmp(argv[i],"--run_type=debug")==0)
            return true;
    }
    return false;
}

void call_so(const char* so_path, const char* func_name, bool debug, const char* exe_dir)
{
    auto logger = get_console_logger();
    void* h = dlopen(so_path, RTLD_LAZY);
    if(!h){
        SPDLOG_LOGGER_ERROR(logger,"open so fail:{} {}",so_path,dlerror());
        return;
    }

    using Func = int(*)(const char*,bool);
    Func f = (Func)dlsym(h,func_name);
    if(!f){
        SPDLOG_LOGGER_ERROR(logger,"find func {} fail:{}",func_name,dlerror());
        dlclose(h);
        return;
    }

    f(exe_dir,debug);
    dlclose(h);
}