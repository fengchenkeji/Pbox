#pragma once
#include <string>
#include "write_log.h"

int run_proot(const char* exe_dir,
              const char* rootfs_path,
              LoggerPtr logger,
              bool debug);
