cmake_minimum_required(VERSION 3.22)
cmake_policy(SET CMP0144 NEW)
project(initialization_fmt CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_definitions(SPDLOG_FMT_EXTERNAL)

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
endif()

add_library(initialization SHARED
    src/initialization.cpp
)

target_include_directories(initialization PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${FMT_INCLUDE}
)

target_link_libraries(initialization
    ${FMT_LIB}
)

add_executable(main src/main.cpp
    src/call_so.cpp
    src/image_db.cpp
)

target_include_directories(main PRIVATE
    ${SPDLOG_INCLUDE}
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(main
    ${SPDLOG_LIB}
    ${FMT_LIB}
    dl
)

add_custom_command(TARGET main POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_BINARY_DIR}/compile_commands.json
        ${CMAKE_SOURCE_DIR}/compile_commands.json
)

install(TARGETS initialization DESTINATION "")
install(TARGETS main DESTINATION "")
