set_source_files_properties(qml/views/ShellView.qml PROPERTIES QT_RESOURCE_ALIAS views/ShellView.qml)
set_source_files_properties(qml/views/HomeView.qml PROPERTIES QT_RESOURCE_ALIAS views/HomeView.qml)
set_source_files_properties(qml/views/DetailView.qml PROPERTIES QT_RESOURCE_ALIAS views/DetailView.qml)
qt_add_qml_module(CaliburnExampleModule
    STATIC
    URI CaliburnExample
    VERSION 1.0
    CLASS_NAME CaliburnExampleModulePlugin
    RESOURCE_PREFIX /qt/qml
    DEPENDENCIES TARGET CaliburnMicroQt
    SOURCES viewmodels/ShellViewModel.h viewmodels/ShellViewModel.cpp
            viewmodels/HomeViewModel.h viewmodels/HomeViewModel.cpp
            viewmodels/DetailViewModel.h viewmodels/DetailViewModel.cpp
            services/CounterService.h services/CounterService.cpp
    QML_FILES qml/views/ShellView.qml qml/views/HomeView.qml qml/views/DetailView.qml
)
target_include_directories(CaliburnExampleModule PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/viewmodels"
    "${CMAKE_CURRENT_SOURCE_DIR}/services"
)
target_link_libraries(CaliburnExampleModule PUBLIC Caliburn::MicroQt PRIVATE Qt6::Quick Qt6::QuickControls2)
