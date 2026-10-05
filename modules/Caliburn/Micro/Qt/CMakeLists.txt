qt_add_qml_module(CaliburnMicroQt
    STATIC
    URI Caliburn.Micro.Qt
    VERSION 1.0
    CLASS_NAME CaliburnMicroQtPlugin
    RESOURCE_PREFIX /qt/qml
    SOURCES
        include/CaliburnMicroQt/ViewModelBase.h
        src/ViewModelBase.cpp
)
add_library(Caliburn::MicroQt ALIAS CaliburnMicroQt)
target_include_directories(CaliburnMicroQt
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include/CaliburnMicroQt"
)
target_link_libraries(CaliburnMicroQt PUBLIC Qt6::Core Qt6::Qml)
