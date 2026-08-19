cmake_minimum_required(VERSION 3.18)
project(Pbox LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -Wall")

# 输出目录配置
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# 全局头文件目录
include_directories(${PROJECT_SOURCE_DIR}/include)

# 查找依赖库
find_package(spdlog REQUIRED)
find_package(CURL REQUIRED)
# libarchive: 多数系统（含 Termux）无 CMake config，直接链接库名
find_library(LIBARCHIVE_LIBRARY NAMES archive libarchive REQUIRED)

# nlohmann_json 是 header-only，Termux 中通过系统包含路径可用
# 如遇找不到头文件，安装: pkg install nlohmann-json

# ===== 动态库：libinitialization.so（proot 加载器）=====
add_library(initialization SHARED
    src/initialization.cpp
)
target_link_libraries(initialization PRIVATE dl spdlog::spdlog)

# ===== 主程序可执行文件 =====
add_executable(main
    src/main.cpp
    src/Cli_menu.cpp
    src/image_db.cpp
    src/write_log.cpp
    src/call_so.cpp
    src/download_rootfs.cpp
    src/container_config.cpp
)

target_link_libraries(main PRIVATE
    dl
    spdlog::spdlog
    CURL::libcurl
    ${LIBARCHIVE_LIBRARY}
)

# ===== 编译后自动复制资源和预编译库到构建目录 =====
add_custom_command(TARGET main POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${PROJECT_SOURCE_DIR}/res $<TARGET_FILE_DIR:main>/res
    COMMENT "Sync res folder to build dir"
)

# 复制预编译的 libproot.so 到构建输出的 lib 目录
add_custom_command(TARGET main POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${PROJECT_SOURCE_DIR}/lib/libproot.so $<TARGET_FILE_DIR:main>/lib/libproot.so
    COMMENT "Copy libproot.so to build dir"
)
