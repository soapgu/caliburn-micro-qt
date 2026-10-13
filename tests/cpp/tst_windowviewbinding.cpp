#include <CaliburnMicroQt/ScreenViewModel.h>
#include "../support/TestCloseWindow.h"
#include <QtTest>
#include <memory>
#include <stdexcept>

class WindowViewBindingTests : public QObject
{
    Q_OBJECT
private slots:
    void noWindowDoesNotDeactivate()
    {
        ScreenViewModel screen;
        screen.activate();
        screen.tryClose(true);
        QVERIFY(screen.isActive());
        QVERIFY(WindowViewBinding::canBind(&screen));
        QCOMPARE(screen.metaObject()->indexOfSignal("closeRequested(DialogResult)"), -1);
    }

    void dialogResults_data()
    {
        QTest::addColumn<int>("decision");
        QTest::newRow("接受") << 1;
        QTest::newRow("取消") << 0;
        QTest::newRow("空结果") << -1;
    }
    void dialogResults()
    {
        QFETCH(int, decision);
        ScreenViewModel screen;
        TestCloseWindow window;
        WindowViewBinding binding(&screen, &window, true);
        const DialogResult result = decision < 0 ? DialogResult{} : DialogResult(decision == 1);
        screen.tryClose(result);
        QCOMPARE(window.calls, 1);
        QCOMPARE(window.result, result ? QVariant(*result) : QVariant());
    }

    void ordinaryWindowIgnoresResultAndDetachStopsClose()
    {
        ScreenViewModel screen;
        TestCloseWindow window;
        WindowViewBinding binding(&screen, &window);
        screen.tryClose(true);
        QCOMPARE(window.calls, 1);
        QVERIFY(!window.result.isValid());
        binding.detach();
        screen.tryClose();
        QCOMPARE(window.calls, 1);
        QVERIFY(WindowViewBinding::canBind(&screen));
    }

    void duplicateBindingDoesNotReplaceWindow()
    {
        ScreenViewModel screen;
        TestCloseWindow first, second;
        WindowViewBinding binding(&screen, &first);
        QVERIFY(!WindowViewBinding::canBind(&screen));
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, WindowViewBinding(&screen, &second));
        screen.tryClose();
        QCOMPARE(first.calls, 1);
        QCOMPARE(second.calls, 0);
    }

    void oldBindingCannotDetachNewWindow()
    {
        ScreenViewModel screen;
        auto first = std::make_unique<TestCloseWindow>();
        auto oldBinding = std::make_unique<WindowViewBinding>(&screen, first.get());
        first.reset();
        QVERIFY(WindowViewBinding::canBind(&screen));
        TestCloseWindow second;
        WindowViewBinding binding(&screen, &second);
        oldBinding.reset();
        screen.tryClose();
        QCOMPARE(second.calls, 1);
    }

    void destroyedModelDoesNotBreakBindingCleanup()
    {
        auto screen = std::make_unique<ScreenViewModel>();
        TestCloseWindow window;
        WindowViewBinding binding(screen.get(), &window);
        screen.reset();
        binding.detach();
        QCOMPARE(window.calls, 0);
    }
};

QTEST_GUILESS_MAIN(WindowViewBindingTests)
#include "tst_windowviewbinding.moc"
