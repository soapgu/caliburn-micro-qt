set_source_files_properties(qml/views/ShellView.qml PROPERTIES QT_RESOURCE_ALIAS views/ShellView.qml)
set_source_files_properties(qml/views/DisplayView.qml PROPERTIES QT_RESOURCE_ALIAS views/DisplayView.qml)
qt_add_qml_module(CaliburnMultiScreenModule
    STATIC URI CaliburnMultiScreen VERSION 1.0
    CLASS_NAME CaliburnMultiScreenModulePlugin
    RESOURCE_PREFIX /qt/qml
    DEPENDENCIES TARGET CaliburnMicroQt
    SOURCES ViewModels.h
    QML_FILES qml/views/ShellView.qml qml/views/DisplayView.qml
)
target_include_directories(CaliburnMultiScreenModule PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(CaliburnMultiScreenModule PUBLIC Caliburn::MicroQt PRIVATE Qt6::Quick)
add_library(CaliburnMultiScreenStartup STATIC AppBootstrapper.h AppBootstrapper.cpp ScreenLayout.h ScreenLayout.cpp)
target_link_libraries(CaliburnMultiScreenStartup PUBLIC CaliburnMultiScreenModule Qt6::Quick)
if(CALIBURN_BUILD_EXAMPLE)
    # 使用显式静态插件导入，避免动态 URL 定位的插件被链接器移除。
    add_executable(CaliburnMultiScreenApp MACOSX_BUNDLE main.cpp)
    target_link_libraries(CaliburnMultiScreenApp PRIVATE CaliburnMultiScreenModuleplugin CaliburnMultiScreenStartup)
    set_target_properties(CaliburnMultiScreenApp PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        MACOSX_BUNDLE_GUI_IDENTIFIER "org.caliburnmicroqt.multiscreen"
        MACOSX_BUNDLE_BUNDLE_NAME "CaliburnMultiScreenApp")
endif()
