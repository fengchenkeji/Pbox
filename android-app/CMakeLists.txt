cmake_minimum_required(VERSION 3.16)
project(PboxAndroid LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Qt6 组件
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Qml
    Quick
    Network
    WebView
    QuickControls2
)

# Android平台特定
if(ANDROID)
    find_package(Qt6 REQUIRED COMPONENTS AndroidExtras)
endif()

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

# QML资源
set(QML_RESOURCES
    qml/main.qml
)

# 生成QML资源qrc
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

if(ANDROID)
    target_link_libraries(PboxAndroid PRIVATE Qt6::AndroidExtras)
endif()

# Android包配置
set(ANDROID_PACKAGE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/android CACHE STRING "")

# 打包proot二进制到APK lib目录
# proot需要从Termux deb提取，命名为libpbox_proot.so
# CI会自动下载并放置
set(PBOX_PROOT_LIB "" CACHE FILEPATH "Path to libpbox_proot.so")
if(PBOX_PROOT_LIB AND EXISTS ${PBOX_PROOT_LIB})
    target_link_libraries(PboxAndroid PRIVATE ${PBOX_PROOT_LIB})
endif()

# Android构建配置
set_target_properties(PboxAndroid PROPERTIES
    ANDROID_PACKAGE_NAME "pbox"
    ANDROID_PACKAGE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/android
    ANDROID_VERSION_CODE 1
    ANDROID_VERSION_NAME "1.0.0"
)

# 安装配置
install(TARGETS PboxAndroid
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
