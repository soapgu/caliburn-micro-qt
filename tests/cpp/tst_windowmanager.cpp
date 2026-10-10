#include <CaliburnMicroQt/WindowManager.h>
#include <CaliburnMicroQt/ViewRegistry.h>
#include <CaliburnMicroQt/Conductor.h>
#include <HomeViewModel.h>
#include "../support/TestWindowManager.h"
#include "../support/DialogWindowSupport.h"
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQmlExtensionPlugin>
#include <QQuickStyle>
#include <QtTest>
#include <stdexcept>

Q_IMPORT_QML_PLUGIN(CaliburnMicroQtPlugin)

class DialogProbe : public ScreenViewModel
{
    Q_OBJECT
public:
    QStringList *events = nullptr;
    bool failActivate = false;
    bool failClose = false;
    bool failGuard = false;
    bool deferred = false;
    bool allowed = true;
    int checks = 0;
    int *checkCounter = nullptr;
    CloseCallback pending;
    std::function<void()> guardAction;
    void canClose(CloseCallback callback) override {
        ++checks;
        if (checkCounter) ++*checkCounter;
        if (guardAction) guardAction();
        if (failGuard) throw std::runtime_error("守卫失败");
        if (deferred) pending = std::move(callback);
        else callback(allowed);
    }
    void decide(bool value) { auto callback = std::move(pending); callback(value); }
protected:
    void onActivate() override {
        if (events) *events << "activate";
        if (failActivate) throw std::runtime_error("激活失败");
    }
    void onDeactivate(bool close) override {
        if (events) *events << (close ? "close" : "deactivate");
        if (failClose) throw std::runtime_error("关闭失败");
    }
};
class WindowProbe : public DialogProbe { Q_OBJECT };
class MissingWindowProbe : public DialogProbe { Q_OBJECT };
class NonWindowProbe : public DialogProbe { Q_OBJECT };
class RemoteWindowProbe : public DialogProbe { Q_OBJECT };
class UnmappedWindowProbe : public DialogProbe { Q_OBJECT };
class UnmappedDialog : public ScreenViewModel { Q_OBJECT };

static QQuickWindow *ordinaryWindow(ScreenViewModel *vm)
{
    for (auto *window : QGuiApplication::allWindows()) {
        if (window->property("viewModel").value<ScreenViewModel *>() == vm)
            return qobject_cast<QQuickWindow *>(window);
    }
    return nullptr;
}

class WindowManagerTests : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase() {
        QQuickStyle::setStyle("Basic");
        QVERIFY(ViewRegistry::registerView<DialogProbe>(QUrl("qrc:/tests/fixtures/Probe.qml")));
        QVERIFY(ViewRegistry::registerView<ScreenViewModel>(QUrl("qrc:/tests/fixtures/Probe.qml")));
        QVERIFY(ViewRegistry::registerView<WindowProbe>(QUrl("qrc:/tests/fixtures/WindowProbe.qml")));
        QVERIFY(ViewRegistry::registerView<MissingWindowProbe>(QUrl("qrc:/tests/missing-window.qml")));
        QVERIFY(ViewRegistry::registerView<NonWindowProbe>(QUrl("qrc:/tests/fixtures/NonVisual.qml")));
        QVERIFY(ViewRegistry::registerView<RemoteWindowProbe>(QUrl("https://example.invalid/Window.qml")));
        QVERIFY(ViewRegistry::freeze());
    }
    // 普通窗口独立于 Bootstrapper；借用 VM、执行守卫并负责正常关闭生命周期。
    void normalWindowWithoutBootstrapper() {
        WindowManager manager;
        IWindowManager &service = manager;
        auto vm = std::make_unique<WindowProbe>();
        vm->deferred = true;
        QSignalSpy deactivated(vm.get(), &ScreenViewModel::deactivated);
        QVERIFY(service.showWindow(QVariant::fromValue(vm.get())));
        QVERIFY(vm->isActive());
        QVERIFY(!manager.busy() && !manager.currentDialog());
        QPointer<QQuickWindow> window = ordinaryWindow(vm.get());
        QVERIFY(window && window->isVisible());
        QObject requester;
        auto dialog = manager.showDialogAsync(std::make_unique<DialogProbe>(), &requester);
        QVERIFY(manager.busy() && dialogWindow(window));
        manager.cancelDialogsFor(&requester);
        QCOMPARE(dialog.result(), DialogResult{});
        vm->tryClose();
        QTRY_COMPARE(vm->checks, 1);
        vm->decide(false);
        QVERIFY(window->isVisible() && vm->isActive());
        QCOMPARE(deactivated.count(), 0);
        vm->tryClose();
        QTRY_COMPARE(vm->checks, 2);
        vm->decide(true);
        QTRY_VERIFY(!window->isVisible());
        QCOMPARE(deactivated.count(), 1);
        QVERIFY(!vm->isActive());
        manager.releaseWindows();
        QVERIFY(!window);
        QVERIFY(vm); // 服务不删除借用的普通窗口 VM。
    }
    void normalWindowRejectsWithoutActivating() {
        WindowManager manager;
        QVERIFY(!manager.showWindow({}));
        QVERIFY(!manager.showWindow(QVariant::fromValue(QStringLiteral("无效类型"))));
        Conductor<ScreenViewModel> conductor;
        conductor.activateItem(std::make_unique<WindowProbe>());
        auto *child = conductor.activeItem();
        QVERIFY(child && child->parentViewModel() == &conductor);
        QVERIFY(!manager.showWindow(QVariant::fromValue(child)));
        QVERIFY(!child->isInitialized());
        WindowProbe vm;
        QVERIFY(manager.showWindow(QVariant::fromValue(&vm)));
        WindowProbe second;
        QVERIFY(!manager.showWindow(QVariant::fromValue(&second)));
        QVERIFY(!second.isInitialized() && vm.isActive());
        manager.releaseWindows();
    }
    void normalWindowLoadFailure_data() {
        QTest::addColumn<QString>("kind");
        for (const auto *kind : {"missing", "nonWindow", "remote", "unmapped", "activate"})
            QTest::newRow(kind) << QString::fromLatin1(kind);
    }
    void normalWindowLoadFailure() {
        QFETCH(QString, kind);
        WindowManager manager;
        std::unique_ptr<DialogProbe> candidate;
        if (kind == "missing") candidate = std::make_unique<MissingWindowProbe>();
        else if (kind == "nonWindow") candidate = std::make_unique<NonWindowProbe>();
        else if (kind == "remote") candidate = std::make_unique<RemoteWindowProbe>();
        else if (kind == "unmapped") candidate = std::make_unique<UnmappedWindowProbe>();
        else candidate = std::make_unique<WindowProbe>();
        QSignalSpy closed(candidate.get(), &ScreenViewModel::deactivated);
        if (kind == "activate") {
            candidate->failActivate = true;
            QVERIFY_THROWS_EXCEPTION(std::runtime_error,
                    (void)manager.showWindow(QVariant::fromValue(candidate.get())));
        } else {
            QVERIFY(!manager.showWindow(QVariant::fromValue(candidate.get())));
        }
        QCOMPARE(closed.count(), int(kind != "remote" && kind != "unmapped"));
        QVERIFY(!candidate->isActive());
        QVERIFY(QGuiApplication::allWindows().isEmpty());
        WindowProbe next;
        QVERIFY(manager.showWindow(QVariant::fromValue(&next))); // 失败释放资源后可重试。
        manager.releaseWindows();
    }
    void normalWindowForcedCleanup_data() {
        QTest::addColumn<QString>("target");
        for (const auto *target : {"release", "manager", "window", "vm"})
            QTest::newRow(target) << QString::fromLatin1(target);
    }
    void normalWindowForcedCleanup() {
        QFETCH(QString, target);
        auto vm = std::make_unique<WindowProbe>();
        auto manager = std::make_unique<WindowManager>();
        QVERIFY(manager->showWindow(QVariant::fromValue(vm.get())));
        QPointer<QQuickWindow> window = ordinaryWindow(vm.get());
        QVERIFY(window);
        QSignalSpy attempted(vm.get(), &ScreenViewModel::attemptingDeactivation);
        connect(vm.get(), &ScreenViewModel::attemptingDeactivation, this, [window](bool close) {
            if (close) QVERIFY(!window);
        });
        if (target == "release") manager->releaseWindows();
        if (target == "manager") manager.reset();
        if (target == "window") delete window.data();
        if (target == "vm") vm.reset();
        QTRY_VERIFY(!window);
        if (vm) QTRY_VERIFY(!vm->isActive());
        QCOMPARE(attempted.count(), int(target != "vm"));
    }
    void normalWindowOldCleanupCannotReleaseNewWindow() {
        WindowManager manager;
        auto previous = std::make_unique<WindowProbe>();
        QVERIFY(manager.showWindow(QVariant::fromValue(previous.get())));
        previous.reset(); // 旧清理排队到旧桥接，不能影响后续窗口。
        manager.releaseWindows();
        WindowProbe next;
        QVERIFY(manager.showWindow(QVariant::fromValue(&next)));
        QPointer<QQuickWindow> window = ordinaryWindow(&next);
        QCoreApplication::processEvents();
        QVERIFY(window && window->isVisible() && next.isActive());
        manager.releaseWindows();
    }
    void normalWindowCloseExceptionIsReportedOnce() {
        WindowManager manager;
        WindowProbe vm; vm.failClose = true;
        QVERIFY(manager.showWindow(QVariant::fromValue(&vm)));
        QSignalSpy attempted(&vm, &ScreenViewModel::attemptingDeactivation);
        QSignalSpy failed(&manager, &WindowManager::windowCleanupFailed);
        ordinaryWindow(&vm)->close();
        QTRY_COMPARE(failed.count(), 1);
        QCOMPARE(attempted.count(), 1);
        manager.releaseWindows();
        QCOMPARE(attempted.count(), 1);
        QCOMPARE(failed.count(), 1);
    }
    void results_data() {
        QTest::addColumn<int>("decision");
        QTest::newRow("接受") << 1;
        QTest::newRow("取消") << 0;
        QTest::newRow("无决定") << -1;
    }
    void results() {
        QFETCH(int, decision);
        WindowProbe ownerModel;
        WindowManager manager;
        auto *owner = showManagedWindow(manager, &ownerModel); QVERIFY(owner);
        QObject requester;
        QStringList events;
        auto vm = std::make_unique<DialogProbe>(); vm->events = &events;
        QPointer<DialogProbe> weak = vm.get();
        QSignalSpy active(vm.get(), &ScreenViewModel::isActiveChanged);
        QSignalSpy closed(vm.get(), &ScreenViewModel::deactivated);
        auto future = manager.showDialogAsync(std::move(vm), &requester);
        QVERIFY(weak && weak->isInitialized() && weak->isActive());
        QCOMPARE(weak->parent(), &manager); QVERIFY(!weak->parentViewModel());
        auto *window = dialogWindow(owner); QVERIFY(window);
        QVERIFY(window != owner && window->isTopLevel());
        QCOMPARE(window->modality(), Qt::ApplicationModal);
        QCOMPARE(window->flags() & Qt::WindowType_Mask, Qt::WindowFlags(Qt::Dialog));
        QPointer<QQuickItem> view = window->property("dialogItem").value<QQuickItem *>();
        QVERIFY(view && view->window() == window);
        connect(weak, &ScreenViewModel::attemptingDeactivation, this, [&](bool close) {
            if (close) QVERIFY(!view); // 关闭钩子前已销毁借用 VM 的 View。
        });
        DialogResult result = decision < 0 ? DialogResult{} : DialogResult(decision == 1);
        manager.closeDialog(weak, result);
        manager.closeDialog(weak, !result.value_or(false));
        QVERIFY(manager.busy() && !future.isFinished());
        QTRY_VERIFY(future.isFinished());
        QCOMPARE(future.result(), result); QVERIFY(!manager.busy());
        QCOMPARE(closed.count(), 1); QCOMPARE(active.count(), 2);
        QVERIFY(!dialogWindow(owner));
        QCOMPARE(events, QStringList({"activate", "close"}));
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete); QVERIFY(!weak);
    }
    void guard_data() {
        QTest::addColumn<bool>("deferred");
        QTest::newRow("立即") << false; QTest::newRow("延后") << true;
    }
    void guard() {
        QFETCH(bool, deferred);
        WindowProbe ownerModel;
        WindowManager manager;
        auto *owner = showManagedWindow(manager, &ownerModel); QVERIFY(owner);
        QObject requester;
        auto vm = std::make_unique<DialogProbe>(); auto *probe = vm.get();
        probe->deferred = deferred; probe->allowed = false;
        QSignalSpy closed(probe, &ScreenViewModel::deactivated);
        auto future = manager.showDialogAsync(std::move(vm), &requester);
        manager.closeDialog(probe, true);
        QTRY_COMPARE(probe->checks, 1);
        manager.closeDialog(probe, false); // 等待中不改写结果。
        if (deferred) probe->decide(false);
        QCoreApplication::processEvents();
        QVERIFY(!future.isFinished() && manager.currentDialog() == probe);
        QVERIFY(dialogWindow(owner)->isVisible()); QCOMPARE(closed.count(), 0);
        probe->allowed = true;
        dialogWindow(owner)->close(); // 原 true 结果已丢弃。
        QTRY_COMPARE(probe->checks, 2);
        if (deferred) probe->decide(true);
        QTRY_VERIFY(future.isFinished()); QCOMPARE(future.result(), DialogResult{});
        QCOMPARE(closed.count(), 1);
    }
    void qmlVetoAndGuardException() {
        WindowProbe ownerModel;
        WindowManager manager;
        auto *owner = showManagedWindow(manager, &ownerModel); QVERIFY(owner);
        QObject requester;
        auto vm = std::make_unique<DialogProbe>(); auto *probe = vm.get(); probe->failGuard = true;
        int checks = 0; probe->checkCounter = &checks;
        auto future = manager.showDialogAsync(std::move(vm), &requester);
        QTest::ignoreMessage(QtWarningMsg, "WindowConductor：关闭守卫失败： 守卫失败");
        manager.closeDialog(probe, true); QTRY_COMPARE(probe->checks, 1);
        QVERIFY(!future.isFinished()); probe->failGuard = false;
        QQmlComponent component(qmlEngine(owner));
        component.setData(R"(
            import QtQuick
            Connections {
                required property Window dialog
                property bool veto: true
                target: dialog
                function onClosing(event) { if (veto) event.accepted = false }
            }
        )", QUrl());
        std::unique_ptr<QObject> handler(component.createWithInitialProperties({
            {"dialog", QVariant::fromValue(dialogWindow(owner))}}));
        QVERIFY2(handler, qPrintable(component.errorString()));
        manager.closeDialog(probe, true); QTRY_COMPARE(probe->checks, 2);
        QVERIFY(!future.isFinished()); QVERIFY(dialogWindow(owner)->isVisible());
        handler->setProperty("veto", false);
        probe->tryClose();
        QTRY_VERIFY(future.isFinished()); QCOMPARE(future.result(), DialogResult{});
        QCOMPARE(checks, 3);
    }
    void directLifecycle() {
        WindowProbe ownerModel;
        WindowManager manager;
        auto *owner = showManagedWindow(manager, &ownerModel); QVERIFY(owner);
        QObject requester;
        auto vm = std::make_unique<DialogProbe>(); auto *probe = vm.get();
        QSignalSpy closed(probe, &ScreenViewModel::deactivated);
        auto future = manager.showDialogAsync(std::move(vm), &requester);
        probe->deactivate(false); QVERIFY(!future.isFinished()); QVERIFY(dialogWindow(owner));
        probe->deactivate(true);
        QTRY_VERIFY(future.isFinished()); QCOMPARE(future.result(), DialogResult{});
        QCOMPARE(probe->checks, 0); QCOMPARE(closed.count(), 2);
    }
    void rejectsAndConsumesCandidates() {
        WindowProbe ownerModel;
        WindowManager manager; QObject requester, parent;
        auto absent = manager.showDialogAsync(std::make_unique<DialogProbe>(), &requester);
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, absent.result());
        auto *owner = showManagedWindow(manager, &ownerModel); QVERIFY(owner);
        auto invalid = manager.showDialogAsync(nullptr, &requester);
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, invalid.result());
        auto unmapped = manager.showDialogAsync(std::make_unique<UnmappedDialog>(), &requester);
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, unmapped.result());
        auto first = manager.showDialogAsync(std::make_unique<DialogProbe>(), &requester);
        auto *current = manager.currentDialog();
        auto next = std::make_unique<DialogProbe>(); QPointer<DialogProbe> consumed = next.get();
        auto busy = manager.showDialogAsync(std::move(next), &requester);
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, busy.result()); QVERIFY(!consumed);
        QCOMPARE(manager.currentDialog(), current);
        manager.cancelDialogsFor(&requester); QCOMPARE(first.result(), DialogResult{});
        auto vm = std::make_unique<DialogProbe>(); consumed = vm.get(); vm->setParent(&parent);
        auto parented = manager.showDialogAsync(std::move(vm), &requester);
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, parented.result()); QVERIFY(!consumed);
        vm = std::make_unique<DialogProbe>(); vm->activate();
        auto active = manager.showDialogAsync(std::move(vm), &requester);
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, active.result());
        auto absentRequester = manager.showDialogAsync(std::make_unique<DialogProbe>(), nullptr);
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, absentRequester.result());
        QThread worker; worker.start(); vm = std::make_unique<DialogProbe>(); consumed = vm.get();
        vm->moveToThread(&worker);
        auto wrongThread = manager.showDialogAsync(std::move(vm), &requester);
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, wrongThread.result());
        QTRY_VERIFY(!consumed); worker.quit(); QVERIFY(worker.wait(3000));
    }
    void forcedCleanupDuringGuard_data() {
        QTest::addColumn<QString>("target");
        for (const auto *target : {"requester", "vm", "dialog", "owner", "manager", "release", "prepare", "cancel"})
            QTest::newRow(target) << QString::fromLatin1(target);
    }
    void forcedCleanupDuringGuard() {
        QFETCH(QString, target);
        WindowProbe ownerModel;
        auto manager = std::make_unique<WindowManager>();
        QPointer<QQuickWindow> owner = showManagedWindow(*manager, &ownerModel); QVERIFY(owner);
        auto requester = std::make_unique<QObject>();
        auto vm = std::make_unique<DialogProbe>(); vm->deferred = true;
        QPointer<DialogProbe> probe = vm.get();
        auto future = manager->showDialogAsync(std::move(vm), requester.get());
        QPointer<QQuickWindow> dialog = dialogWindow(owner); QVERIFY(dialog);
        QPointer<QQuickItem> view = dialog->property("dialogItem").value<QQuickItem *>(); QVERIFY(view);
        connect(probe, &ScreenViewModel::attemptingDeactivation, this, [view](bool close) {
            if (close) QVERIFY(!view);
        });
        manager->closeDialog(probe, true); QTRY_COMPARE(probe->checks, 1);
        auto late = std::move(probe->pending);
        if (target == "requester") requester.reset();
        if (target == "vm") delete probe.data();
        if (target == "dialog") delete dialog.data();
        if (target == "owner") delete owner.data();
        if (target == "manager") manager.reset();
        if (target == "release") manager->releaseWindows();
        if (target == "prepare") manager->prepareForShutdown();
        if (target == "cancel") manager->cancelDialogsFor(requester.get());
        QTRY_VERIFY(future.isFinished()); QCOMPARE(future.result(), DialogResult{});
        QVERIFY(!dialog && !view); if (manager) QVERIFY(!manager->busy());
        late(true); // 已销毁的桥接不会被迟到许可访问。
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete); QVERIFY(!probe);
    }
    void lifecycleExceptions_data() {
        QTest::addColumn<bool>("activation");
        QTest::newRow("激活异常") << true; QTest::newRow("关闭异常") << false;
    }
    void lifecycleExceptions() {
        QFETCH(bool, activation);
        WindowProbe ownerModel;
        WindowManager manager;
        auto *owner = showManagedWindow(manager, &ownerModel); QVERIFY(owner);
        QObject requester;
        QStringList events;
        auto vm = std::make_unique<DialogProbe>(); QPointer<DialogProbe> probe = vm.get();
        vm->events = &events; vm->failActivate = activation; vm->failClose = !activation;
        QSignalSpy attempted(probe, &ScreenViewModel::attemptingDeactivation);
        auto future = manager.showDialogAsync(std::move(vm), &requester);
        if (!activation) manager.closeDialog(probe, true);
        QTRY_VERIFY(future.isFinished());
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, future.result());
        QCOMPARE(events, QStringList({"activate", "close"})); QCOMPARE(attempted.count(), 1);
        QVERIFY(!manager.busy() && !dialogWindow(owner));
    }
    void nestedDialogIsBusy() {
        WindowProbe ownerModel;
        WindowManager manager;
        auto *owner = showManagedWindow(manager, &ownerModel); QVERIFY(owner);
        QObject requester;
        auto vm = std::make_unique<DialogProbe>(); auto *probe = vm.get();
        QFuture<DialogResult> nested;
        probe->guardAction = [&] {
            nested = manager.showDialogAsync(std::make_unique<DialogProbe>(), &requester);
        };
        auto future = manager.showDialogAsync(std::move(vm), &requester);
        manager.closeDialog(probe, true);
        QTRY_VERIFY(future.isFinished()); QCOMPARE(future.result(), DialogResult(true));
        QVERIFY(nested.isFinished());
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, nested.result());
    }
    void destroyedVmCleanupCannotCloseNextDialog() {
        WindowProbe ownerModel;
        WindowManager manager;
        auto *owner = showManagedWindow(manager, &ownerModel); QVERIFY(owner);
        QObject requester;
        auto first = manager.showDialogAsync(std::make_unique<DialogProbe>(), &requester);
        delete manager.currentDialog(); // 清理在旧窗口上下文中排队。
        manager.cancelDialogsFor(&requester); // 提前完成并销毁旧窗口。
        QCOMPARE(first.result(), DialogResult{});
        auto second = manager.showDialogAsync(std::make_unique<DialogProbe>(), &requester);
        QCoreApplication::processEvents();
        QVERIFY(manager.busy() && !second.isFinished());
        manager.cancelDialogsFor(&requester); QCOMPARE(second.result(), DialogResult{});
    }
    void failedExternalCloseIsNotRetried() {
        WindowProbe ownerModel;
        WindowManager manager;
        auto *owner = showManagedWindow(manager, &ownerModel); QVERIFY(owner);
        QObject requester;
        auto vm = std::make_unique<DialogProbe>(); auto *probe = vm.get(); vm->failClose = true;
        QSignalSpy attempted(probe, &ScreenViewModel::attemptingDeactivation);
        auto future = manager.showDialogAsync(std::move(vm), &requester);
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, probe->deactivate(true));
        QVERIFY(!future.isFinished() && dialogWindow(owner)->isVisible());
        manager.cancelDialogsFor(&requester);
        QCOMPARE(attempted.count(), 1); QCOMPARE(future.result(), DialogResult{});
    }
    void homeIgnoresOldResultAndDestroyedRequester() {
        auto windows = std::make_shared<TestWindowManager>();
        windows->autoComplete = false; windows->deferCancellation = true;
        auto service = std::make_shared<CounterService>();
        HomeViewModel home(service, windows); home.activate(); home.add(2); home.reset();
        home.deactivate(); home.activate(); home.reset();
        windows->deferred->addResult(DialogResult(true)); windows->deferred->finish();
        QCoreApplication::processEvents();
        QCOMPARE(home.count(), 2); QVERIFY(home.resetPending());
        windows->complete(false); QTRY_VERIFY(!home.resetPending());
        WindowProbe ownerModel;
        auto manager = std::make_shared<WindowManager>();
        auto *owner = showManagedWindow(*manager, &ownerModel); QVERIFY(owner);
        auto requester = std::make_unique<HomeViewModel>(service, manager);
        requester->activate(); requester->reset(); QVERIFY(manager->busy());
        requester.reset(); QVERIFY(!manager->busy()); QCOMPARE(service->count(), 2);
    }
    void homeConfirmationAndCancellation() {
        auto windows = std::make_shared<TestWindowManager>(); windows->autoComplete = false;
        auto service = std::make_shared<CounterService>();
        HomeViewModel home(service, windows); home.activate(); home.add(3);
        home.reset(); home.reset();
        QCOMPARE(windows->requests, 1);
        QVERIFY(home.resetPending()); QVERIFY(!home.canReset()); QCOMPARE(home.count(), 3);
        windows->complete(false);
        QTRY_VERIFY(!home.resetPending()); QCOMPARE(home.count(), 3);
        home.reset(); windows->complete(true);
        QTRY_COMPARE(home.count(), 0);
        home.add(2); home.reset(); home.deactivate(false);
        QCOMPARE(windows->cancellations, 1);
        QVERIFY(!home.resetPending()); QCOMPARE(home.count(), 2);
        home.activate(); home.reset();
        QTest::ignoreMessage(QtWarningMsg, "Home：重置确认失败"); windows->fail();
        QTRY_VERIFY(!home.resetPending()); QCOMPARE(home.count(), 2);
        home.reset(); windows->complete(true); QTRY_COMPARE(home.count(), 0);
    }
};
QTEST_MAIN(WindowManagerTests)
#include "tst_windowmanager.moc"
