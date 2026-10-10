set_source_files_properties(qml/ViewHost.qml PROPERTIES QT_RESOURCE_ALIAS ViewHost.qml)
find_package(Qt6 6.8.3 REQUIRED COMPONENTS QuickControls2)
set_source_files_properties(qml/DialogHost.qml PROPERTIES QT_RESOURCE_ALIAS DialogHost.qml)
set_source_files_properties(qml/ConfirmActionView.qml PROPERTIES QT_RESOURCE_ALIAS ConfirmActionView.qml)
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
        include/CaliburnMicroQt/IChild.h
        include/CaliburnMicroQt/IParent.h
        include/CaliburnMicroQt/IGuardClose.h
        include/CaliburnMicroQt/ICloseStrategy.h
        include/CaliburnMicroQt/DefaultCloseStrategy.h
        src/DefaultCloseStrategy.cpp
        include/CaliburnMicroQt/IConductor.h
        include/CaliburnMicroQt/ConductorBase.h
        src/ConductorBase.cpp
        include/CaliburnMicroQt/ScreenViewModel.h
        src/ScreenViewModel.cpp
        include/CaliburnMicroQt/ConductorViewModelBase.h
        include/CaliburnMicroQt/Conductor.h
        src/ConductorViewModelBase.cpp
        include/CaliburnMicroQt/ConductorCollectionOneActiveViewModelBase.h
        include/CaliburnMicroQt/ConductorCollectionOneActive.h
        src/ConductorCollectionOneActiveViewModelBase.cpp
        include/CaliburnMicroQt/ViewRegistry.h
        src/ViewRegistry.cpp
        src/ViewHostState.h
        src/ViewHostState.cpp
        include/CaliburnMicroQt/IWindowManager.h
        include/CaliburnMicroQt/WindowManager.h
        include/CaliburnMicroQt/ConfirmationRequest.h
        include/CaliburnMicroQt/ConfirmActionViewModel.h
        src/WindowManager.cpp
        src/ConfirmActionViewModel.cpp
        src/DialogHostState.h
        src/DialogHostState.cpp
    QML_FILES qml/ViewHost.qml qml/DialogHost.qml qml/ConfirmActionView.qml
)
add_library(Caliburn::MicroQt ALIAS CaliburnMicroQt)
target_include_directories(CaliburnMicroQt
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include/CaliburnMicroQt"
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_link_libraries(CaliburnMicroQt PUBLIC Qt6::Core Qt6::Qml PRIVATE Qt6::Quick Qt6::QuickControls2)
