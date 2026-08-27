cmake_minimum_required(VERSION 3.18)
project(Pbox LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Os -Wall -fvisibility=hidden")
# 使用预编译的 spdlog 共享库，避免头文件模板膨胀
add_compile_definitions(SPDLOG_COMPILED_LIB SPDLOG_FMT_EXTERNAL)

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

# 交叉编译时手动指定依赖库，避免termux cmake配置文件硬编码路径问题
option(PBOX_MANUAL_DEPS "Manually specify dependency paths instead of find_package" OFF)
if(PBOX_MANUAL_DEPS)
    message(STATUS "Using manual dependency paths")
    if(NOT DEFINED PBOX_DEPS_ROOT)
        message(FATAL_ERROR "PBOX_MANUAL_DEPS=ON requires PBOX_DEPS_ROOT")
    endif()
    include_directories(${PBOX_DEPS_ROOT}/include)
    link_directories(${PBOX_DEPS_ROOT}/lib)
    set(SPDLOG_LIB spdlog fmt)
    set(CURL_LIB curl)
else()
    find_package(spdlog REQUIRED)
    find_package(CURL REQUIRED)
    set(SPDLOG_LIB spdlog::spdlog)
    set(CURL_LIB CURL::libcurl)
endif()

add_library(initialization SHARED
    src/initialization.cpp
    src/container_config.cpp
)
target_link_libraries(initialization PRIVATE dl ${SPDLOG_LIB})

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
    ${SPDLOG_LIB}
    ${CURL_LIB}
)
# 强制导出主程序所有符号，供 libproot.so 通过 dlsym 查找 main
target_link_options(main PRIVATE -Wl,--export-dynamic -Wl,-E)
set_target_properties(main PROPERTIES
    BUILD_RPATH "${CMAKE_BINARY_DIR}/lib"
    INSTALL_RPATH "${TERMUX_PREFIX}/opt/Pbox/lib"
)

# 复制资源文件夹
add_custom_command(TARGET main POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${PROJECT_SOURCE_DIR}/res $<TARGET_FILE_DIR:main>/res
    COMMENT "Sync res folder to build dir"
)

# 预编译so文件复制：使用 add_custom_target 确保每次构建都执行
set(PBOX_SO_ROOT "${PROJECT_SOURCE_DIR}/lib/${PBOX_ABI_DIR}")
message(STATUS "Prebuilt so root dir: ${PBOX_SO_ROOT}")

add_custom_target(copy_prebuilt_so ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/lib
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${PBOX_SO_ROOT} ${CMAKE_BINARY_DIR}/lib
    COMMENT "Copy all prebuilt so files from ${PBOX_ABI_DIR} to build/lib"
)

# 主程序依赖 so 复制目标
add_dependencies(main copy_prebuilt_so)

# 编译后自动 strip，去除调试符号，大幅减小体积
if(CMAKE_STRIP)
    add_custom_command(TARGET main POST_BUILD
        COMMAND ${CMAKE_STRIP} --strip-unneeded $<TARGET_FILE:main>
        COMMENT "Strip main executable"
    )
    add_custom_command(TARGET initialization POST_BUILD
        COMMAND ${CMAKE_STRIP} --strip-unneeded $<TARGET_FILE:initialization>
        COMMENT "Strip libinitialization.so"
    )
endif()
