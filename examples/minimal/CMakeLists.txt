include("${PROJECT_SOURCE_DIR}/cmake/ExampleDependencies.cmake")
add_subdirectory(CaliburnExample)
add_library(CaliburnExampleLogging STATIC
    app/logging/LoggingRuntime.h
    app/logging/LoggingRuntime.cpp
)
target_include_directories(CaliburnExampleLogging PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/app/logging")
target_link_libraries(CaliburnExampleLogging PUBLIC Qt6::Core PRIVATE spdlog::spdlog)
add_library(CaliburnExampleComposition STATIC
    app/ViewModelComposition.h
    app/ViewModelComposition.cpp
)
target_include_directories(CaliburnExampleComposition PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/app")
target_link_libraries(CaliburnExampleComposition
    PUBLIC CaliburnExampleModule
    PRIVATE Caliburn::BoostDI
)
if(CALIBURN_BUILD_EXAMPLE)
    qt_add_executable(CaliburnExampleApp MACOSX_BUNDLE
        app/main.cpp
        app/AppBootstrapper.h
        app/AppBootstrapper.cpp
    )
    # Qt 导入扫描已加入两个静态 QML 插件，无需重复显式链接。
    target_link_libraries(CaliburnExampleApp PRIVATE
        CaliburnExampleComposition
        CaliburnExampleLogging
        Qt6::QuickControls2
    )
    set_target_properties(CaliburnExampleApp PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        MACOSX_BUNDLE_GUI_IDENTIFIER "org.caliburnmicroqt.example"
        MACOSX_BUNDLE_BUNDLE_NAME "CaliburnExampleApp"
    )
endif()
