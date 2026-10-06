add_subdirectory(CaliburnExample)
add_subdirectory("${PROJECT_SOURCE_DIR}/third_party/boost-di" "${CMAKE_CURRENT_BINARY_DIR}/boost-di")
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
    target_link_libraries(CaliburnExampleApp PRIVATE
        CaliburnExampleComposition
        CaliburnMicroQtplugin
        CaliburnExampleModuleplugin
        Qt6::QuickControls2
    )
    set_target_properties(CaliburnExampleApp PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        MACOSX_BUNDLE_GUI_IDENTIFIER "org.caliburnmicroqt.example"
        MACOSX_BUNDLE_BUNDLE_NAME "CaliburnExampleApp"
    )
endif()
