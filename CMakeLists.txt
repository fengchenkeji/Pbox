cmake_minimum_required(VERSION 3.18)
project(Pbox LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -Wall")

# 优先使用ANDROID_ABI；否则使用传入的PBOX_ABI
if(ANDROID_ABI)
    set(PBOX_ABI "${ANDROID_ABI}")
elseif(NOT PBOX_ABI)
    message(WARNING "PBOX_ABI not set, default to armeabi‑v7a, pass -DPBOX_ABI=arm64‑v8a for 64bit")
    set(PBOX_ABI "armeabi‑v7a")
endif()

message(STATUS "PBOX target ABI: ${PBOX_ABI}")

# 输出目录：库输出到 build/lib/${PBOX_ABI}
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/${PBOX_ABI})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# 头文件目录
include_directories(${PROJECT_SOURCE_DIR}/include)

# 依赖库
find_package(spdlog REQUIRED)
find_package(CURL REQUIRED)

# ===== 动态库：libinitialization.so =====
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
    src/container_config.cpp
)
target_link_libraries(main PRIVATE
    dl
    spdlog::spdlog
    CURL::libcurl
)

# ===== 编译后复制资源 =====
add_custom_command(TARGET main POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${PROJECT_SOURCE_DIR}/res $<TARGET_FILE_DIR:main>/res
    COMMENT "Sync res folder to build dir"
)

# 从源码 lib/${PBOX_ABI}/libproot.so 复制到 build/lib/${PBOX_ABI}/libproot.so
add_custom_command(TARGET main POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:main>/lib/${PBOX_ABI}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${PROJECT_SOURCE_DIR}/lib/${PBOX_ABI}/libproot.so
    $<TARGET_FILE_DIR:main>/lib/${PBOX_ABI}/libproot.so
    COMMENT "Copy libproot.so (ABI:${PBOX_ABI}) to build/lib/${PBOX_ABI}"
)
