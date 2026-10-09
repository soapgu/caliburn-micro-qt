qt_add_executable(CaliburnParentProtocolTests cpp/tst_parent_protocol.cpp)
target_link_libraries(CaliburnParentProtocolTests PRIVATE Caliburn::MicroQt Qt6::Test)
add_test(NAME parent_protocol COMMAND CaliburnParentProtocolTests)
set_tests_properties(parent_protocol PROPERTIES TIMEOUT 30)

qt_add_executable(CaliburnCollectionTests cpp/tst_collection.cpp)
target_link_libraries(CaliburnCollectionTests PRIVATE Caliburn::MicroQt Qt6::Test)
add_test(NAME collection COMMAND CaliburnCollectionTests)
set_tests_properties(collection PROPERTIES TIMEOUT 30)

qt_add_executable(CaliburnCounterServiceTests cpp/tst_counterservice.cpp)
target_link_libraries(CaliburnCounterServiceTests PRIVATE CaliburnExampleModule Qt6::Test)
add_test(NAME counterservice COMMAND CaliburnCounterServiceTests)

qt_add_executable(CaliburnConductorTests cpp/tst_conductor.cpp)
target_link_libraries(CaliburnConductorTests PRIVATE Caliburn::MicroQt Qt6::Test)
add_test(NAME conductor COMMAND CaliburnConductorTests)
set_tests_properties(conductor PROPERTIES TIMEOUT 30)

qt_add_executable(CaliburnCoreTests cpp/tst_core.cpp)
target_link_libraries(CaliburnCoreTests PRIVATE CaliburnExampleModule Qt6::Test)
add_test(NAME core COMMAND CaliburnCoreTests)

qt_add_executable(CaliburnCompositionTests cpp/tst_composition.cpp)
target_link_libraries(CaliburnCompositionTests PRIVATE CaliburnExampleComposition Qt6::Test)
add_test(NAME composition COMMAND CaliburnCompositionTests)

qt_add_executable(CaliburnQmlTests qml/tst_qml.cpp)
# 故意非法的 QML 仅作为原始测试资源，不参与模块编译或 qmllint。
qt_add_resources(CaliburnQmlTests host_fixtures
    PREFIX /tests
    FILES fixtures/MissingProperty.qml fixtures/MissingRequired.qml
          fixtures/NonVisual.qml fixtures/SyntaxError.qml fixtures/Mismatch.qml
)
target_link_libraries(CaliburnQmlTests PRIVATE
    CaliburnExampleComposition CaliburnMicroQtplugin CaliburnExampleModuleplugin
    Qt6::Test Qt6::Quick Qt6::QuickControls2
)
add_test(NAME qml COMMAND CaliburnQmlTests)
set_tests_properties(qml PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")

qt_add_executable(CaliburnBootstrapperTests
    cpp/tst_bootstrapper.cpp
    ../examples/minimal/app/AppBootstrapper.h
    ../examples/minimal/app/AppBootstrapper.cpp
)
qt_add_resources(CaliburnBootstrapperTests bootstrapper_fixtures
    PREFIX /tests
    FILES fixtures/NonVisual.qml
)
target_link_libraries(CaliburnBootstrapperTests PRIVATE
    CaliburnExampleComposition CaliburnMicroQtplugin CaliburnExampleModuleplugin
    Qt6::Test Qt6::Quick Qt6::QuickControls2
)
# 注册表映射与冻结状态均为进程级；各场景需要不同映射或空表，独立进程验收。
foreach(scenario IN ITEMS
    success configureQuery lateRegistration exitCode configureFailure missingFactory factoryException nullRoot
    missingMapping remoteView missingResource nonWindow noRoot ignoredLoadFailure
    duplicateRoot exitException closeException
)
    add_test(NAME bootstrapper_${scenario}
        COMMAND CaliburnBootstrapperTests "startupAndCleanup:${scenario}")
    set_tests_properties(bootstrapper_${scenario} PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
        TIMEOUT 15
    )
endforeach()
add_test(NAME bootstrapper_example COMMAND CaliburnBootstrapperTests exampleStartup)
set_tests_properties(bootstrapper_example PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
    TIMEOUT 15
)

qt_add_executable(CaliburnWindowManagerTests cpp/tst_windowmanager.cpp)
target_include_directories(CaliburnWindowManagerTests PRIVATE
    "${PROJECT_SOURCE_DIR}/modules/Caliburn/Micro/Qt/src")
target_link_libraries(CaliburnWindowManagerTests PRIVATE CaliburnExampleModule Qt6::Test)
add_test(NAME windowmanager COMMAND CaliburnWindowManagerTests)
set_tests_properties(windowmanager PROPERTIES TIMEOUT 30)

qt_add_executable(CaliburnDialogTests qml/tst_dialog.cpp)
qt_add_resources(CaliburnDialogTests dialog_fixtures
    PREFIX /tests
    FILES fixtures/CustomDialog.qml fixtures/NonVisual.qml
          fixtures/SyntaxError.qml fixtures/MissingProperty.qml
)
target_link_libraries(CaliburnDialogTests PRIVATE
    CaliburnExampleComposition CaliburnMicroQtplugin CaliburnExampleModuleplugin
    Qt6::Test Qt6::Quick Qt6::QuickControls2
)
add_test(NAME dialog COMMAND CaliburnDialogTests)
set_tests_properties(dialog PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
    TIMEOUT 60
)
