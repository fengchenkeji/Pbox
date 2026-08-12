#pragma once
#ifdef __cplusplus
extern "C" {
#endif

/**
 * 返回码说明
 * -1：dlopen打开libproot.so失败
 * -2：dlsym查找main符号失败
 * >=0：proot程序正常退出返回值
 */
int run_proot(const char* exe_dir, const char* rootfs_path);

#ifdef __cplusplus
}
#endif
