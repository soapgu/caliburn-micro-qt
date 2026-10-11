# 纯 C++ 测试不加载 QML；普通目标避免 Qt 的导入扫描额外链接插件及模块库。
add_executable(CaliburnParentProtocolTests cpp/tst_parent_protocol.cpp)
target_link_libraries(CaliburnParentProtocolTests PRIVATE Caliburn::MicroQt Qt6::Test)
add_test(NAME parent_protocol COMMAND CaliburnParentProtocolTests)
set_tests_properties(parent_protocol PROPERTIES TIMEOUT 30)

add_executable(CaliburnCollectionTests cpp/tst_collection.cpp)
target_link_libraries(CaliburnCollectionTests PRIVATE Caliburn::MicroQt Qt6::Test)
add_test(NAME collection COMMAND CaliburnCollectionTests)
set_tests_properties(collection PROPERTIES TIMEOUT 30)

add_executable(CaliburnCounterServiceTests cpp/tst_counterservice.cpp)
target_link_libraries(CaliburnCounterServiceTests PRIVATE CaliburnExampleModule Qt6::Test)
add_test(NAME counterservice COMMAND CaliburnCounterServiceTests)

add_executable(CaliburnConductorTests cpp/tst_conductor.cpp)
target_link_libraries(CaliburnConductorTests PRIVATE Caliburn::MicroQt Qt6::Test)
add_test(NAME conductor COMMAND CaliburnConductorTests)
set_tests_properties(conductor PROPERTIES TIMEOUT 30)

add_executable(CaliburnCoreTests cpp/tst_core.cpp)
target_link_libraries(CaliburnCoreTests PRIVATE CaliburnExampleModule Qt6::Test)
add_test(NAME core COMMAND CaliburnCoreTests)

add_executable(CaliburnCompositionTests cpp/tst_composition.cpp)
target_link_libraries(CaliburnCompositionTests PRIVATE CaliburnExampleComposition Qt6::Test)
add_test(NAME composition COMMAND CaliburnCompositionTests)

# QML 测试已通过 Q_IMPORT_QML_PLUGIN 注册插件，使用普通目标避免再次自动扫描。
add_executable(CaliburnQmlTests qml/tst_qml.cpp)
# 故意非法的 QML 仅作为原始测试资源，不参与模块编译或 qmllint。
qt_add_resources(CaliburnQmlTests host_fixtures
    PREFIX /tests
    FILES fixtures/MissingProperty.qml fixtures/MissingRequired.qml
          fixtures/NonVisual.qml fixtures/SyntaxError.qml fixtures/Mismatch.qml
)
# 示例由动态 URL 加载，显式链接其插件；Qt 会传递框架插件依赖。
# 插件放在依赖其模块库的组合库之前，避免静态库被重复加入链接行。
target_link_libraries(CaliburnQmlTests PRIVATE
    CaliburnExampleModuleplugin CaliburnExampleComposition
    Qt6::Test Qt6::Quick Qt6::QuickControls2
)
add_test(NAME qml COMMAND CaliburnQmlTests)
set_tests_properties(qml PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")

add_executable(CaliburnBootstrapperTests
    cpp/tst_bootstrapper.cpp
    ../examples/minimal/app/AppBootstrapper.h
    ../examples/minimal/app/AppBootstrapper.cpp
)
qt_add_resources(CaliburnBootstrapperTests bootstrapper_fixtures
    PREFIX /tests
    FILES fixtures/NonVisual.qml fixtures/HiddenShell.qml
)
target_link_libraries(CaliburnBootstrapperTests PRIVATE
    CaliburnExampleModuleplugin CaliburnExampleComposition
    Qt6::Test Qt6::Quick Qt6::QuickControls2
)
# 注册表映射与冻结状态均为进程级；各场景需要不同映射或空表，独立进程验收。
foreach(scenario IN ITEMS
    hiddenWindow retainedService success injectedService explicitOptions emptyFactory hostFailure configureQuery lateRegistration exitCode configureFailure missingFactory factoryException nullRoot
    missingMapping remoteView missingResource nonWindow noRoot ignoredLoadFailure
    duplicateRoot exitException closeException startupException windowCloseException externalCloseException
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
add_test(NAME bootstrapper_defaultStartup COMMAND CaliburnBootstrapperTests defaultStartup)
set_tests_properties(bootstrapper_defaultStartup PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software" TIMEOUT 15)
foreach(scenario IN ITEMS home accept cancel escape rootTryClose modelClose ordinaryDeactivate quit forcedExit forcedExitDialog inactiveDetail busy missingHome showFailure)
    add_test(NAME rootwindow_${scenario} COMMAND CaliburnBootstrapperTests "rootWindowClose:${scenario}")
    set_tests_properties(rootwindow_${scenario} PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software" TIMEOUT 15)
endforeach()

add_executable(CaliburnWindowManagerTests cpp/tst_windowmanager.cpp)
qt_add_resources(CaliburnWindowManagerTests probe_fixture PREFIX /tests
    FILES fixtures/Probe.qml fixtures/WindowProbe.qml fixtures/NonVisual.qml)
target_include_directories(CaliburnWindowManagerTests PRIVATE
    "${PROJECT_SOURCE_DIR}/modules/Caliburn/Micro/Qt/src")
target_link_libraries(CaliburnWindowManagerTests PRIVATE
    CaliburnMicroQtplugin CaliburnExampleModule Qt6::Test Qt6::Quick Qt6::QuickControls2)
add_test(NAME windowmanager COMMAND CaliburnWindowManagerTests)
set_tests_properties(windowmanager PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software" TIMEOUT 60)

add_executable(CaliburnDialogTests qml/tst_dialog.cpp)
qt_add_resources(CaliburnDialogTests dialog_fixtures
    PREFIX /tests
    FILES fixtures/CustomDialog.qml fixtures/NonVisual.qml
          fixtures/SyntaxError.qml fixtures/MissingProperty.qml
)
target_link_libraries(CaliburnDialogTests PRIVATE
    CaliburnExampleModuleplugin CaliburnExampleComposition
    Qt6::Test Qt6::Quick Qt6::QuickControls2
)
add_test(NAME dialog COMMAND CaliburnDialogTests)
set_tests_properties(dialog PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
    TIMEOUT 60
)

# 框架默认映射、应用覆盖和无效覆盖使用独立进程，避免静态注册表互相影响。
# 测试通过 Q_IMPORT_QML_PLUGIN 注册框架插件，普通目标避免 Qt 再次自动链接。
add_executable(CaliburnFrameworkDialogTests qml/tst_framework_dialog.cpp)
qt_add_resources(CaliburnFrameworkDialogTests framework_dialog_fixtures
    PREFIX /tests FILES fixtures/OverrideConfirm.qml fixtures/FrameworkWindow.qml)
# 插件依赖框架库，按插件在前、框架库在后的顺序链接，避免重复静态库。
target_link_libraries(CaliburnFrameworkDialogTests PRIVATE
    CaliburnMicroQtplugin Caliburn::MicroQt Qt6::Test Qt6::Quick Qt6::QuickControls2)
add_test(NAME framework_dialog COMMAND CaliburnFrameworkDialogTests)
set_tests_properties(framework_dialog PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software" TIMEOUT 30)
foreach(mapping IN ITEMS override missing)
    add_test(NAME framework_dialog_${mapping} COMMAND CaliburnFrameworkDialogTests registryMapping)
    set_tests_properties(framework_dialog_${mapping} PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;CMQT_CONFIRM_MAPPING=${mapping}"
        TIMEOUT 15)
endforeach()

add_executable(CaliburnGuardCloseTests cpp/tst_guardclose.cpp)
target_link_libraries(CaliburnGuardCloseTests PRIVATE Caliburn::MicroQt Qt6::Test)
add_test(NAME guardclose COMMAND CaliburnGuardCloseTests)
set_tests_properties(guardclose PROPERTIES TIMEOUT 30)

add_executable(CaliburnWindowConductorTests cpp/tst_windowconductor.cpp)
target_include_directories(CaliburnWindowConductorTests PRIVATE
    "${PROJECT_SOURCE_DIR}/modules/Caliburn/Micro/Qt/src")
target_link_libraries(CaliburnWindowConductorTests PRIVATE Caliburn::MicroQt Qt6::Test Qt6::Quick)
add_test(NAME windowconductor COMMAND CaliburnWindowConductorTests)
set_tests_properties(windowconductor PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software" TIMEOUT 30)
