cmake_minimum_required(VERSION 3.22)
cmake_policy(SET CMP0144 NEW)
project(initialization_fmt CXX)

# C++标准全局配置
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
# 统一输出目录
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/)

add_compile_definitions(SPDLOG_FMT_EXTERNAL)

# 区分Termux环境 / 本地离线库
if(DEFINED ENV{TERMUX_VERSION})
    find_package(fmt REQUIRED)
    find_package(spdlog REQUIRED)
    set(FMT_INCLUDE ${fmt_INCLUDE_DIRS})
    set(FMT_LIB fmt::fmt)
    set(SPDLOG_INCLUDE ${spdlog_INCLUDE_DIRS})
    set(SPDLOG_LIB spdlog::spdlog)
else()
    set(FMT_ROOT "/storage/emulated/0/Pbox/fmt/1")
    set(SPDLOG_ROOT "/storage/emulated/0/Pbox/spdlog/")

    find_library(FMT_LIB
        NAMES fmt libfmt
        PATHS ${FMT_ROOT}/lib
        NO_DEFAULT_PATH
    )
    if(NOT FMT_LIB)
        message(FATAL_ERROR "Cannot find fmt library at ${FMT_ROOT}/lib")
    endif()

    find_library(SPDLOG_LIB
        NAMES spdlog libspdlog
        PATHS ${SPDLOG_ROOT}/lib
        NO_DEFAULT_PATH
    )
    if(NOT SPDLOG_LIB)
        message(FATAL_ERROR "Cannot find spdlog library at ${SPDLOG_ROOT}/lib")
    endif()

    set(FMT_INCLUDE ${FMT_ROOT}/include)
    set(SPDLOG_INCLUDE ${SPDLOG_ROOT}/include)
endif()

# 公共静态库 common
add_library(common STATIC
    src/call_so.cpp
    src/image_db.cpp
    src/write_log.cpp
)
target_include_directories(common PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${FMT_INCLUDE}
    ${SPDLOG_INCLUDE}
)
target_link_libraries(common PRIVATE ${FMT_LIB} ${SPDLOG_LIB})

# 动态库 initialization
add_library(initialization SHARED src/initialization.cpp)
target_include_directories(initialization PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(initialization PRIVATE common)

# 主程序main
add_executable(main src/main.cpp)
target_include_directories(main PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(main PRIVATE common dl)

# 复制编译数据库
add_custom_command(TARGET main POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    ${CMAKE_BINARY_DIR}/compile_commands.json
    ${CMAKE_SOURCE_DIR}/compile_commands.json
    COMMENT "Copy compile_commands.json to project root"
)

# 每次编译同步res到bin目录（解决路径找不到images.txt核心bug）
add_custom_command(TARGET main PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/bin/res
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/res ${CMAKE_BINARY_DIR}/res
    COMMENT "Sync res folder to bin dir"
)
