cmake_minimum_required(VERSION 3.21)
project(CaliburnMicroQt VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CALIBURN_BUILD_EXAMPLE "构建单页面示例" ${PROJECT_IS_TOP_LEVEL})
option(CALIBURN_BUILD_TESTS "构建框架与示例测试" ${PROJECT_IS_TOP_LEVEL})

find_package(Qt6 6.8.3 REQUIRED COMPONENTS Core Qml Quick)
qt_standard_project_setup(REQUIRES 6.8)
if(NOT DEFINED QT_QML_OUTPUT_DIRECTORY)
    set(QT_QML_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/qml")
endif()
# Qt Creator 从 CMake 缓存读取导入路径，以识别构建生成的 qmldir 和 qmltypes。
set(QML_IMPORT_PATH "${QT_QML_OUTPUT_DIRECTORY}" CACHE STRING "Qt Creator 使用的 QML 模块导入目录")

add_subdirectory(modules/Caliburn/Micro/Qt)
if(CALIBURN_BUILD_EXAMPLE OR CALIBURN_BUILD_TESTS)
    find_package(Qt6 6.8.3 REQUIRED COMPONENTS Quick QuickControls2)
    add_subdirectory(examples/minimal)
endif()
if(CALIBURN_BUILD_TESTS)
    enable_testing()
    find_package(Qt6 6.8.3 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()
