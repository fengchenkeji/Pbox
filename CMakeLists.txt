cmake_minimum_required(VERSION 3.18)
project(Pbox LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_FLAGS "-O2 -Wall")

# 输出目录配置
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# 全局头文件目录
include_directories(${PROJECT_SOURCE_DIR}/include)

# 动态库：libinitialization.so proot加载器
add_library(initialization SHARED
    src/initialization.cpp
)
target_link_libraries(initialization PRIVATE dl)

# 主程序可执行文件
add_executable(main
    src/main.cpp
    src/Cli_menu.cpp
    src/image_db.cpp
    src/write_log.cpp
    src/call_so.cpp
)
find_package(spdlog REQUIRED)
target_link_libraries(main PRIVATE dl spdlog::spdlog)

# 编译后自动复制资源文件夹
add_custom_command(TARGET main POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${PROJECT_SOURCE_DIR}/res $<TARGET_FILE_DIR:main>/res
    COMMENT "Sync res folder to build dir"
)
