cmake_minimum_required(VERSION 3.16)

project(Pbox VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.12.0 REQUIRED COMPONENTS Quick)

qt_standard_project_setup(REQUIRES 6.12.0)

qt_add_executable(appPbox
    main.cpp
)

qt_add_qml_module(appPbox
    URI Pbox
    VERSION 1.0
    QML_FILES
        Main.qml
)

set_target_properties(Pbox PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(appPbox
    PRIVATE Qt6::Quick
)

include(GNUInstallDirs)