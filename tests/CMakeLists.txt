qt_add_executable(CaliburnCoreTests cpp/tst_core.cpp)
target_link_libraries(CaliburnCoreTests PRIVATE CaliburnExampleModule Qt6::Test)
add_test(NAME core COMMAND CaliburnCoreTests)

qt_add_executable(CaliburnQmlTests qml/tst_qml.cpp)
target_link_libraries(CaliburnQmlTests PRIVATE
    CaliburnExampleModule CaliburnMicroQtplugin CaliburnExampleModuleplugin
    Qt6::Test Qt6::Quick Qt6::QuickControls2
)
add_test(NAME qml COMMAND CaliburnQmlTests)
set_tests_properties(qml PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
