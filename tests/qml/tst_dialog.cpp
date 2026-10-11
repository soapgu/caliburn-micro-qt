#include "../support/DialogWindowSupport.h"
#include <CaliburnMicroQt/WindowManager.h>
#include <CaliburnMicroQt/ConfirmActionViewModel.h>
#include <CaliburnMicroQt/ViewRegistry.h>
#include <ViewModelComposition.h>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlExtensionPlugin>
#include <QLibraryInfo>
#include <QQuickItem>
#include <QQuickWindow>
#include <QQuickStyle>
#include <QtTest>
#include <memory>
#include <stdexcept>

Q_IMPORT_QML_PLUGIN(CaliburnMicroQtPlugin)
Q_IMPORT_QML_PLUGIN(CaliburnExampleModulePlugin)

class CustomDialogVm : public ScreenViewModel
{
    Q_OBJECT
public:
    Q_INVOKABLE void finish() { tryClose(true); }
};
class MissingDialog : public ScreenViewModel { Q_OBJECT };
class NonVisualDialog : public ScreenViewModel { Q_OBJECT };
class SyntaxDialog : public ScreenViewModel { Q_OBJECT };
class BadInjectionDialog : public ScreenViewModel { Q_OBJECT };

static void click(QQuickWindow *window, QQuickItem *item)
{
    QVERIFY(QTest::qWaitForWindowExposed(item->window()));
    if (!dialogWindow(window) || item->window() != window) {
        item->window()->requestActivate();
        QVERIFY(QTest::qWaitForWindowActive(item->window()));
    }
    QTest::mouseClick(item->window(), Qt::LeftButton, Qt::NoModifier,
        item->mapToScene(QPointF(item->width()/2, item->height()/2)).toPoint());
}

class DialogTests : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase() {
        QQuickStyle::setStyle("Basic");
        qmlRegisterUncreatableType<CustomDialogVm>("DialogTest", 1, 0, "CustomDialogVm", "测试 VM");
        QVERIFY(ViewRegistry::registerView<ShellViewModel>(QUrl("qrc:/qt/qml/CaliburnExample/views/ShellView.qml")));
        QVERIFY(ViewRegistry::registerView<HomeViewModel>(QUrl("qrc:/qt/qml/CaliburnExample/views/HomeView.qml")));
        QVERIFY(ViewRegistry::registerView<DetailViewModel>(QUrl("qrc:/qt/qml/CaliburnExample/views/DetailView.qml")));
        QVERIFY(ViewRegistry::registerView<CustomDialogVm>(QUrl("qrc:/tests/fixtures/CustomDialog.qml")));
        QVERIFY(ViewRegistry::registerView<MissingDialog>(QUrl("qrc:/tests/absent.qml")));
        QVERIFY(ViewRegistry::registerView<NonVisualDialog>(QUrl("qrc:/tests/fixtures/NonVisual.qml")));
        QVERIFY(ViewRegistry::registerView<SyntaxDialog>(QUrl("qrc:/tests/fixtures/SyntaxError.qml")));
        QVERIFY(ViewRegistry::registerView<BadInjectionDialog>(QUrl("qrc:/tests/fixtures/MissingProperty.qml")));
        QVERIFY(ViewRegistry::freeze());
    }

    void homeAcceptCancelAndModalInput() {
        auto windows = std::make_shared<WindowManager>();
        auto shell = buildShell(windows); shell->activate(); shell->home()->add(3);
        WindowCleanup cleanup{*windows};
        auto *window = showManagedWindow(*windows, shell.get()); QVERIFY(window);
        QSignalSpy warnings(qmlEngine(window), &QQmlEngine::warnings);
        QVERIFY(QTest::qWaitForWindowExposed(window));
        auto *input = window->findChild<QQuickItem *>("focusInput"); QVERIFY(input);
        input->forceActiveFocus();
        shell->home()->reset();
        QTRY_VERIFY(windows->busy());
        auto *cancel = dialogControl(window, "dialogCancel");
        auto *accept = dialogControl(window, "dialogAccept");
        QVERIFY(cancel && accept);
        QTRY_VERIFY(cancel->hasActiveFocus());
        QTest::keyClick(dialogWindow(window) ? dialogWindow(window) : window, Qt::Key_Tab); QTRY_VERIFY(accept->hasActiveFocus());
        QTest::keyClick(dialogWindow(window) ? dialogWindow(window) : window, Qt::Key_Tab); QTRY_VERIFY(cancel->hasActiveFocus());
        QTest::keyClick(dialogWindow(window) ? dialogWindow(window) : window, Qt::Key_Backtab); QTRY_VERIFY(accept->hasActiveFocus());
        QTest::keyClick(dialogWindow(window) ? dialogWindow(window) : window, Qt::Key_2); QCOMPARE(shell->home()->count(), 3);
        auto *navigation = window->findChild<QQuickItem *>("showDetail"); QVERIFY(navigation);
        click(window, navigation); QVERIFY(!shell->detail());
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, QPoint(3,3));
        QVERIFY(windows->busy());
        cancel->forceActiveFocus();
        QTest::keyClick(dialogWindow(window) ? dialogWindow(window) : window, Qt::Key_Return);
        QTRY_VERIFY(!shell->home()->resetPending());
        QCOMPARE(shell->home()->count(), 3);
        QTRY_VERIFY(input->hasActiveFocus());
        shell->home()->reset();
        accept = dialogControl(window, "dialogAccept"); QVERIFY(accept);
        click(window, accept);
        QTRY_COMPARE(shell->home()->count(), 0);
        QVERIFY(!windows->busy());
        QCOMPARE(warnings.count(), 0);
        shell->deactivate(true); window->close();
    }
    void escapeAndPageDeactivation() {
        auto windows = std::make_shared<WindowManager>();
        auto shell = buildShell(windows); shell->activate(); shell->home()->add(2);
        WindowCleanup cleanup{*windows};
        auto *window = showManagedWindow(*windows, shell.get()); QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));
        shell->home()->reset();
        QTRY_VERIFY(windows->busy());
        QVERIFY(QTest::qWaitForWindowActive(dialogWindow(window)));
        QTest::keyClick(dialogWindow(window), Qt::Key_Escape);
        QTRY_VERIFY(!windows->busy());
        QCOMPARE(shell->home()->count(), 2);
        shell->home()->reset();
        shell->showDetail();
        QTRY_VERIFY(!windows->busy());
        QVERIFY(!shell->home()->resetPending());
        shell->detail()->goBack();
        QCOMPARE(shell->home()->count(), 2);
        shell->deactivate(true); window->close();
    }
    void detailConfirmationAndFocus() {
        auto windows = std::make_shared<WindowManager>();
        auto shell = buildShell(windows); shell->activate(); shell->home()->add(2);
        WindowCleanup cleanup{*windows};
        auto *window = showManagedWindow(*windows, shell.get()); QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));
        window->requestActivate(); QVERIFY(QTest::qWaitForWindowActive(window));
        shell->showDetail();
        auto *detail = shell->detail();
        QTRY_VERIFY(window->findChild<QQuickItem *>("goBack"));
        QPointer<QQuickItem> back = window->findChild<QQuickItem *>("goBack");
        back->forceActiveFocus(); click(window, back.data());
        QTRY_VERIFY(windows->busy());
        auto *cancel = dialogControl(window, "dialogCancel");
        QVERIFY(cancel); QTRY_VERIFY(cancel->hasActiveFocus());
        QTest::keyClick(dialogWindow(window) ? dialogWindow(window) : window, Qt::Key_2); QCOMPARE(detail->count(), 2);
        QVERIFY(QTest::qWaitForWindowActive(dialogWindow(window)));
        QTest::keyClick(dialogWindow(window), Qt::Key_Escape);
        QTRY_VERIFY(!windows->busy()); QCOMPARE(shell->activeItem(), detail);
        QTRY_VERIFY(back->hasActiveFocus());
        click(window, back.data()); QTRY_VERIFY(windows->busy());
        cancel = dialogControl(window, "dialogCancel"); QVERIFY(cancel);
        click(window, cancel); QTRY_VERIFY(!windows->busy()); QCOMPARE(shell->activeItem(), detail);
        QSignalSpy closed(detail, &ScreenViewModel::deactivated);
        click(window, back.data()); QTRY_VERIFY(windows->busy());
        auto *accept = dialogControl(window, "dialogAccept"); QVERIFY(accept);
        click(window, accept);
        QTRY_VERIFY(shell->activeItem() == shell->home()); QCOMPARE(closed.count(), 1);
        QCOMPARE(shell->home()->count(), 2); QTRY_VERIFY(!back);
        QTest::keyClick(dialogWindow(window) ? dialogWindow(window) : window, Qt::Key_2); QTRY_COMPARE(shell->home()->count(), 4);
        shell->deactivate(true); window->close();
    }

    void customVmAndViewBeforeVmDestruction() {
        auto windows = std::make_shared<WindowManager>();
        auto shell = buildShell(windows); shell->activate();
        WindowCleanup cleanup{*windows};
        auto *window = showManagedWindow(*windows, shell.get()); QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));
        auto *manager = windows.get();
        auto vm = std::make_unique<CustomDialogVm>();
        QPointer<CustomDialogVm> weak = vm.get();
        auto future = manager->showDialogAsync(std::move(vm), shell.get());
        auto *button = dialogControl(window, "customDialogAccept"); QVERIFY(button);
        auto *host = dialogWindow(window); QVERIFY(host);
        QPointer<QQuickItem> view = host->property("dialogItem").value<QQuickItem *>(); QVERIFY(view);
        QStringList order;
        connect(view, &QObject::destroyed, this, [&] { order << "view"; });
        connect(weak, &QObject::destroyed, this, [&] { order << "vm"; });
        click(window, button);
        QTRY_VERIFY(future.isFinished()); QCOMPARE(future.result(), DialogResult(true));
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(!weak && !view);
        QCOMPARE(order, QStringList({"view", "vm"}));
        shell->deactivate(true); window->close();
    }
    void loadFailures_data() {
        QTest::addColumn<int>("kind");
        QTest::newRow("缺失资源") << 0;
        QTest::newRow("非 Item") << 1;
        QTest::newRow("语法错误") << 2;
        QTest::newRow("注入失败") << 3;
    }
    void loadFailures() {
        QFETCH(int, kind);
        auto windows = std::make_shared<WindowManager>();
        auto shell = buildShell(windows); shell->activate();
        WindowCleanup cleanup{*windows};
        auto *window = showManagedWindow(*windows, shell.get()); QVERIFY(window);
        std::unique_ptr<ScreenViewModel> vm;
        if (kind == 0) vm = std::make_unique<MissingDialog>();
        if (kind == 1) vm = std::make_unique<NonVisualDialog>();
        if (kind == 2) vm = std::make_unique<SyntaxDialog>();
        if (kind == 3) vm = std::make_unique<BadInjectionDialog>();
        auto future = windows->showDialogAsync(std::move(vm), shell.get());
        QTRY_VERIFY(future.isFinished());
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, future.result());
        QVERIFY(!windows->busy());
        shell->deactivate(true); window->close();
    }
    void releasingWindowsCompletesRequest() {
        auto windows = std::make_shared<WindowManager>();
        auto shell = buildShell(windows); shell->activate();
        WindowCleanup cleanup{*windows};
        QVERIFY(showManagedWindow(*windows, shell.get()));
        auto *manager = windows.get();
        auto future = manager->showDialogAsync(std::make_unique<CustomDialogVm>(), shell.get());
        windows->releaseWindows();
        QTRY_VERIFY(future.isFinished());
        QCOMPARE(future.result(), DialogResult{});
        QVERIFY(!manager->busy()); shell->deactivate(true);
    }
};
QTEST_MAIN(DialogTests)
#include "tst_dialog.moc"
