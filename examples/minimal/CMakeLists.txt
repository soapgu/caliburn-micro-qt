add_subdirectory(CaliburnExample)
if(CALIBURN_BUILD_EXAMPLE)
    qt_add_executable(CaliburnExampleApp MACOSX_BUNDLE app/main.cpp)
    target_link_libraries(CaliburnExampleApp PRIVATE
        CaliburnExampleModule
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
