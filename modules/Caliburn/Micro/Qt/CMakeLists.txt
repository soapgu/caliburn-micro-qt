set_source_files_properties(qml/ViewHost.qml PROPERTIES QT_RESOURCE_ALIAS ViewHost.qml)
qt_add_qml_module(CaliburnMicroQt
    STATIC
    URI Caliburn.Micro.Qt
    VERSION 1.0
    CLASS_NAME CaliburnMicroQtPlugin
    RESOURCE_PREFIX /qt/qml
    SOURCES
        include/CaliburnMicroQt/BootstrapperBase.h
        src/BootstrapperBase.cpp
        include/CaliburnMicroQt/ViewModelBase.h
        src/ViewModelBase.cpp
        include/CaliburnMicroQt/ScreenViewModel.h
        src/ScreenViewModel.cpp
        include/CaliburnMicroQt/ViewRegistry.h
        src/ViewRegistry.cpp
        src/ViewHostState.h
        src/ViewHostState.cpp
    QML_FILES qml/ViewHost.qml
)
add_library(Caliburn::MicroQt ALIAS CaliburnMicroQt)
target_include_directories(CaliburnMicroQt
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include/CaliburnMicroQt"
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_link_libraries(CaliburnMicroQt PUBLIC Qt6::Core Qt6::Qml PRIVATE Qt6::Quick)
