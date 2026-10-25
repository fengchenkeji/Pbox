cmake_minimum_required(VERSION 3.16)

project(Pbox VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============ 找 Qt6（桌面 6.12 / Android 6.7.3 均满足下限 6.5） ============
find_package(Qt6 6.5 REQUIRED COMPONENTS
    Core
    Gui
    Qml
    Quick
    QuickControls2
    Network
    Concurrent
)

qt_standard_project_setup(REQUIRES 6.5)

# ============ 主程序 ============
qt_add_executable(appPbox
    main.cpp
    src/pbox_paths.cpp
    src/pty_process.cpp
    src/container_manager.cpp
    src/terminal_bridge.cpp
)

# ============ QML 模块（Main.qml + C++ 类型注册） ============
qt_add_qml_module(appPbox
    URI Pbox
    VERSION 1.0
    QML_FILES
        Main.qml
)

# ============ 链接 ============
target_link_libraries(appPbox
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::Network
        Qt6::Concurrent
)

# ============ Android 打包 source 目录（Manifest/res/assets 打进 APK） ============
set_target_properties(appPbox PROPERTIES
    QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android"
)

# ============ 安装规则（桌面端用） ============
include(GNUInstallDirs)
install(TARGETS appPbox
    BUNDLE DESTINATION .
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)
