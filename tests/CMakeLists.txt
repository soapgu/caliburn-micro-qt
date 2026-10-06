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
# 注册表首次查询后全局冻结；各启动场景必须在独立进程验收。
foreach(scenario IN ITEMS
    success exitCode configureFailure missingFactory factoryException nullRoot
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
