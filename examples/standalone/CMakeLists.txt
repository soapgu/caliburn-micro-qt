cmake_minimum_required(VERSION 3.21)
project(CaliburnConsumer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.8.3 REQUIRED COMPONENTS Core Qml Quick QuickControls2)
qt_standard_project_setup(REQUIRES 6.8)
set(QT_QML_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/qml")
set(QML_IMPORT_PATH "${QT_QML_OUTPUT_DIRECTORY}" CACHE STRING "Qt Creator 的 QML 导入目录")

# 可将本目录复制到框架仓库之外，再指定框架源码路径独立构建。
set(CALIBURN_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../.." CACHE PATH "Caliburn.Micro.Qt 源码目录")
set(CALIBURN_BUILD_EXAMPLE OFF CACHE BOOL "不引入框架示例" FORCE)
set(CALIBURN_BUILD_TESTS OFF CACHE BOOL "不引入框架测试" FORCE)
add_subdirectory("${CALIBURN_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/caliburn")

# 消费工程不应依赖示例、日志出口或容器，也不应下载它们的第三方依赖。
foreach(forbidden IN ITEMS CaliburnExampleModule CaliburnExampleComposition CaliburnExampleLogging CaliburnBoostDI spdlog)
    if(TARGET ${forbidden})
        message(FATAL_ERROR "独立消费工程意外引入了示例依赖：${forbidden}")
    endif()
endforeach()

set_source_files_properties(qml/views/ShellView.qml PROPERTIES QT_RESOURCE_ALIAS views/ShellView.qml)
set_source_files_properties(qml/views/HomeView.qml PROPERTIES QT_RESOURCE_ALIAS views/HomeView.qml)
qt_add_qml_module(CaliburnConsumerModule
    STATIC
    URI CaliburnConsumer
    VERSION 1.0
    CLASS_NAME CaliburnConsumerModulePlugin
    RESOURCE_PREFIX /qt/qml
    DEPENDENCIES TARGET CaliburnMicroQt
    SOURCES HomeViewModel.h ShellViewModel.h ShellViewModel.cpp
    QML_FILES qml/views/ShellView.qml qml/views/HomeView.qml
)
target_include_directories(CaliburnConsumerModule PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(CaliburnConsumerModule PUBLIC Caliburn::MicroQt PRIVATE Qt6::Quick Qt6::QuickControls2)

# 约定定位通过 URL 加载视图，显式保留静态插件；普通目标避免重复导入扫描。
add_executable(CaliburnConsumerApp MACOSX_BUNDLE main.cpp AppBootstrapper.h)
target_link_libraries(CaliburnConsumerApp PRIVATE CaliburnConsumerModuleplugin CaliburnConsumerModule Qt6::Quick Qt6::QuickControls2)
set_target_properties(CaliburnConsumerApp PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    MACOSX_BUNDLE_GUI_IDENTIFIER "org.caliburnmicroqt.consumer"
)

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 6.8.3 REQUIRED COMPONENTS Test)
    add_executable(CaliburnConsumerTests tests/tst_consumer.cpp AppBootstrapper.h)
    target_link_libraries(CaliburnConsumerTests PRIVATE CaliburnConsumerModuleplugin CaliburnConsumerModule Qt6::Test Qt6::Quick Qt6::QuickControls2)
    add_test(NAME standalone_consumer COMMAND CaliburnConsumerTests)
    set_tests_properties(standalone_consumer PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
        TIMEOUT 30
    )
endif()
