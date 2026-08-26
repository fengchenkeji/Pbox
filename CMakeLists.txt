cmake_minimum_required(VERSION 3.18)
project(Pbox LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -Wall")

# 架构识别，定义对应文件列表
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^aarch64|arm64")
    set(PBOX_ABI_DIR "arm64‑v8a")
    set(SO_FILES
        libproot.so
        libproot-loader.so
        libproot-loader32.so
        libtalloc.so.2
    )
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^arm")
    set(PBOX_ABI_DIR "armeabi‑v7a")
    set(SO_FILES
        libproot.so
        libproot-loader.so
        libtalloc.so.2
    )
else()
    message(WARNING "Unknown arch, fallback to armeabi‑v7a")
    set(PBOX_ABI_DIR "armeabi‑v7a")
    set(SO_FILES
        libproot.so
        libproot-loader.so
        libtalloc.so.2
    )
endif()

message(STATUS "PBOX target ABI dir: ${PBOX_ABI_DIR}")
message(STATUS "Files to copy: ${SO_FILES}")

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

include_directories(${PROJECT_SOURCE_DIR}/include)

find_package(spdlog REQUIRED)
find_package(CURL REQUIRED)

add_library(initialization SHARED
    src/initialization.cpp
    src/container_config.cpp
)
target_link_libraries(initialization PRIVATE dl spdlog::spdlog)

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

# 复制资源文件夹
add_custom_command(TARGET main POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${PROJECT_SOURCE_DIR}/res $<TARGET_FILE_DIR:main>/res
    COMMENT "Sync res folder to build dir"
)

# 逐个复制文件，每个文件都拼接完整源路径
set(PBOX_SO_ROOT "${PROJECT_SOURCE_DIR}/lib/${PBOX_ABI_DIR}")
message(STATUS "Prebuilt so root dir: ${PBOX_SO_ROOT}")

# 先创建目标目录
add_custom_command(TARGET main POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/lib
    COMMENT "Create output lib dir"
)

# 循环复制每个架构对应的so文件
foreach(so_file ${SO_FILES})
    add_custom_command(TARGET main POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${PBOX_SO_ROOT}/${so_file}
            ${CMAKE_BINARY_DIR}/lib/
        COMMENT "Copy ${so_file}"
    )
endforeach()
