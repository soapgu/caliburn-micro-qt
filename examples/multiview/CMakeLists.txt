foreach(view IN ITEMS ShellView HomeView Home/Master Home/Detail)
    set_source_files_properties(qml/views/${view}.qml PROPERTIES QT_RESOURCE_ALIAS views/${view}.qml)
endforeach()
qt_add_qml_module(CaliburnMultiViewModule
    STATIC URI CaliburnMultiView VERSION 1.0
    CLASS_NAME CaliburnMultiViewModulePlugin
    RESOURCE_PREFIX /qt/qml
    DEPENDENCIES TARGET CaliburnMicroQt
    SOURCES ViewModels.h
    QML_FILES qml/views/ShellView.qml qml/views/HomeView.qml
              qml/views/Home/Master.qml qml/views/Home/Detail.qml
)
target_include_directories(CaliburnMultiViewModule PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(CaliburnMultiViewModule PUBLIC Caliburn::MicroQt PRIVATE Qt6::Quick Qt6::QuickControls2)
if(CALIBURN_BUILD_EXAMPLE)
    add_executable(CaliburnMultiViewApp MACOSX_BUNDLE main.cpp AppBootstrapper.h)
    target_link_libraries(CaliburnMultiViewApp PRIVATE CaliburnMultiViewModuleplugin CaliburnMultiViewModule Qt6::QuickControls2)
    set_target_properties(CaliburnMultiViewApp PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        MACOSX_BUNDLE_GUI_IDENTIFIER "org.caliburnmicroqt.multiview")
endif()
