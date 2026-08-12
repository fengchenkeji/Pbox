#include "initialization.h"
#include <dlfcn.h>
#include <string>

using ProotMain = int (*)(int argc, char** argv);

extern "C" int run_proot(const char* exe_dir, const char* rootfs_path)
{
    std::string proot_so_path = std::string(exe_dir) + "/lib/libproot.so";
    void* handle = dlopen(proot_so_path.c_str(), RTLD_NOW | RTLD_GLOBAL);
    if (!handle)
    {
        return -1;
    }

    auto proot_main = reinterpret_cast<ProotMain>(dlsym(handle, "main"));
    if (!proot_main)
    {
        dlclose(handle);
        return -2;
    }

    const char* argv[] = {
        "proot",
        "-0",
        "-r", rootfs_path,
        "-b", "/dev",
        "/bin/sh"
    };
    int argc = sizeof(argv) / sizeof(char*);
    int ret_code = proot_main(argc, const_cast<char**>(argv));
    dlclose(handle);
    return ret_code;
}
