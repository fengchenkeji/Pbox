#include "initialization.h"
#include "container_config.h"

#include <iostream>
#include <vector>
#include <string>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/prctl.h>
#include <dlfcn.h>

int run_proot(const char* exe_dir,
              const char* rootfs_path,
              LoggerPtr logger,
              bool debug)
{
    std::string exeDir(exe_dir);
    std::string rootfsDir(rootfs_path);

    ContainerConfig cfg;
    bool ok = load_container_config(exeDir, rootfsDir, cfg, logger, debug);
    if(!ok)
    {
        DBG(logger, debug, "加载容器配置失败");
        return -10;
    }

    std::vector<const char*> argv;
    argv.push_back("proot");
    argv.push_back("--link2symlink");
    argv.push_back("-0");
    argv.push_back("-r");
    argv.push_back(rootfsDir.c_str());

    argv.push_back("-b"); argv.push_back("/dev");
    argv.push_back("-b"); argv.push_back("/proc");

    std::string devshm = rootfsDir + "/root:/dev/shm";
    argv.push_back("-b"); argv.push_back(devshm.c_str());

    for(auto &mnt : cfg.mounts)
    {
        argv.push_back("-b");
        argv.push_back(mnt.c_str());
    }

    argv.push_back("-w");
    argv.push_back("/root");

    argv.push_back("/usr/bin/env");
    argv.push_back("-i");
    argv.push_back("HOME=/root");
    argv.push_back("PATH=/usr/local/sbin:/usr/local/bin:/bin:/usr/bin:/sbin:/usr/sbin:/usr/games:/usr/local/games");

    const char* term_env = getenv("TERM");
    std::string termStr = term_env ? std::string("TERM=") + term_env : std::string("TERM=xterm");
    argv.push_back(termStr.c_str());

    argv.push_back("LANG=C.UTF-8");
    argv.push_back("/bin/bash");
    argv.push_back("--login");
    argv.push_back(nullptr);

    int argc = static_cast<int>(argv.size()) -1;

    for(auto& pf : cfg.portForwards)
    {
        DBG(logger, debug, "启动端口转发 host:{} -> container:{}", pf.hostPort, pf.containerPort);
        pid_t pid = fork();
        if(pid == 0)
        {
            prctl(PR_SET_PDEATHSIG, SIGKILL);
            execlp("socat",
                "socat",
                "TCP-LISTEN:%d,fork,reuseaddr",
                ("TCP:127.0.0.1:" + std::to_string(pf.containerPort)).c_str(),
                nullptr);
            _exit(1);
        }
    }

    void* proot_handle = dlopen("./libproot.so", RTLD_LAZY);
    if(!proot_handle)
    {
        DBG(logger, debug, "dlopen libproot.so failed: {}", dlerror());
        return -1;
    }
    using ProotMain = int (*)(int, char**);
    auto proot_main = reinterpret_cast<ProotMain>(dlsym(proot_handle, "main"));
    if(!proot_main)
    {
        DBG(logger, debug, "找不到proot main符号: {}", dlerror());
        dlclose(proot_handle);
        return -2;
    }

    proot_main(argc, const_cast<char**>(argv.data()));
    dlclose(proot_handle);
    return 0;
}
