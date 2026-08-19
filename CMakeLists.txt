cmake_minimum_required(VERSION 3.18)
project(Pbox LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -Wall")

# 输出目录
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# 头文件目录
include_directories(${PROJECT_SOURCE_DIR}/include)

# 依赖库
find_package(spdlog REQUIRED)
find_package(CURL REQUIRED)
find_library(LIBARCHIVE_LIBRARY NAMES archive libarchive REQUIRED)

# ===== 动态库：libinitialization.so =====
# 包含 container_config.cpp，因为 initialization.cpp 调用 load_container_config
add_library(initialization SHARED
    src/initialization.cpp
    src/container_config.cpp
)
target_link_libraries(initialization PRIVATE dl spdlog::spdlog)

# ===== 主程序 =====
add_executable(main
    src/main.cpp
    src/Cli_menu.cpp
    src/image_db.cpp
    src/write_log.cpp
    src/call_so.cpp
    src/download_rootfs.cpp
)
target_link_libraries(main PRIVATE
    dl
    spdlog::spdlog
    CURL::libcurl
    ${LIBARCHIVE_LIBRARY}
)

# ===== 编译后复制资源 =====
add_custom_command(TARGET main POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${PROJECT_SOURCE_DIR}/res $<TARGET_FILE_DIR:main>/res
    COMMENT "Sync res folder to build dir"
)

add_custom_command(TARGET main POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${PROJECT_SOURCE_DIR}/lib/libproot.so $<TARGET_FILE_DIR:main>/lib/libproot.so
    COMMENT "Copy libproot.so to build dir"
)
