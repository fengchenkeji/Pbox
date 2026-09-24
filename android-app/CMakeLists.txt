cmake_minimum_required(VERSION 3.16)
project(PboxAndroid LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Qt6 组件（去掉不需要的WebView和AndroidExtras）
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Qml
    Quick
    Network
    QuickControls2
)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

# 应用源码
set(APP_SOURCES
    src/main.cpp
    src/pbox_paths.cpp
    src/pbox_paths.h
    src/pty_process.cpp
    src/pty_process.h
    src/container_manager.cpp
    src/container_manager.h
)

# 生成可执行文件
qt_add_executable(PboxAndroid
    ${APP_SOURCES}
)

# QML模块
qt_add_qml_module(PboxAndroid
    URI Pbox
    VERSION 1.0
    QML_FILES
        qml/main.qml
)

# 链接Qt库
target_link_libraries(PboxAndroid PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Network
    Qt6::QuickControls2
)

# Android包配置
set(ANDROID_PACKAGE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/android CACHE STRING "")

# 注意：proot二进制(libpbox_proot.so)不链接，而是作为native library打包进APK
# 通过CI复制到android-build/lib/arm64-v8a/目录

# Android构建配置
set_target_properties(PboxAndroid PROPERTIES
    ANDROID_PACKAGE_NAME "pbox"
    ANDROID_PACKAGE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/android
    ANDROID_VERSION_CODE 1
    ANDROID_VERSION_NAME "1.0.0"
)
