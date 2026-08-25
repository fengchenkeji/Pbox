cmake_minimum_required(VERSION 3.18)
project(Pbox LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -Wall")

# 自动识别架构：termux aarch64 -> arm64‑v8a；arm -> armeabi‑v7a
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^aarch64|arm64")
    set(PBOX_NATIVE_ABI "arm64‑v8a")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^arm")
    set(PBOX_NATIVE_ABI "armeabi‑v7a")
else()
    message(WARNING "Unknown arch, fallback to armeabi‑v7a")
    set(PBOX_NATIVE_ABI "armeabi‑v7a")
endif()

# 允许外部覆盖 ABI，优先 ANDROID_ABI，其次 -DPBOX_ABI，最后自动识别
if(ANDROID_ABI)
    set(PBOX_ABI "${ANDROID_ABI}")
elseif(PBOX_ABI)
    set(PBOX_ABI "${PBOX_ABI}")
else()
    set(PBOX_ABI "${PBOX_NATIVE_ABI}")
endif()

message(STATUS "PBOX target ABI: ${PBOX_ABI}")

# 库输出目录：直接 build/lib，不再带abi子文件夹
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
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

# 自动根据识别到的ABI复制 libproot.so 到 build/lib/ （不再嵌套abi子目录）
set(PROOT_SOURCE_SO "${PROJECT_SOURCE_DIR}/lib/${PBOX_ABI}/libproot.so")

add_custom_command(TARGET main POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/lib
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${PROOT_SOURCE_SO}
    ${CMAKE_BINARY_DIR}/lib/libproot.so
    COMMENT "Copy libproot.so (ABI:${PBOX_ABI}) -> build/lib/libproot.so"
)
