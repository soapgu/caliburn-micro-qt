#include <CaliburnMicroQt/WindowManager.h>
#include <CaliburnMicroQt/ConfirmActionViewModel.h>
#include <CaliburnMicroQt/ViewRegistry.h>
#include <HomeViewModel.h>
#include "DialogHostState.h"
#include "../support/TestWindowManager.h"
#include <QtTest>
#include <stdexcept>

class DialogProbe : public ScreenViewModel
{
    Q_OBJECT
public:
    QStringList *events = nullptr;
    bool failActivate = false;
    bool failClose = false;
protected:
    void onActivate() override {
        if (events) *events << "activate";
        if (failActivate) throw std::runtime_error("激活失败");
    }
    void onDeactivate(bool) override {
        if (events) *events << "close";
        if (failClose) throw std::runtime_error("关闭失败");
    }
};
class UnmappedDialog : public ScreenViewModel { Q_OBJECT };

static void connectHost(DialogHostState &host, WindowManager &manager)
{
    host.setAvailable(true);
    host.setManager(&manager);
    QObject::connect(&host, &DialogHostState::hideRequested, &host,
                     [&host](const QString &id) { host.released(id); });
}

class WindowManagerTests : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase() {
        QVERIFY(ViewRegistry::registerView<DialogProbe>(QUrl("qrc:/tests/Probe.qml")));
        QVERIFY(ViewRegistry::registerView<ScreenViewModel>(QUrl("qrc:/tests/Probe.qml")));
        QVERIFY(ViewRegistry::registerView<ConfirmActionViewModel>(QUrl("qrc:/tests/Confirm.qml")));
        QVERIFY(ViewRegistry::freeze());
    }
    void automaticallyActivatesPlainScreen() {
        WindowManager manager; DialogHostState host; connectHost(host, manager);
        QObject requester;
        auto vm = std::make_unique<ScreenViewModel>();
        QPointer<ScreenViewModel> weak = vm.get();
        QSignalSpy initialized(vm.get(), &ScreenViewModel::isInitializedChanged);
        QSignalSpy active(vm.get(), &ScreenViewModel::isActiveChanged);
        auto future = manager.showDialogAsync(std::move(vm), &requester);
        QVERIFY(weak && weak->isInitialized() && weak->isActive());
        QCOMPARE(initialized.count(), 1);
        QCOMPARE(active.count(), 1);
        manager.closeDialog(weak, false);
        QCOMPARE(future.result(), DialogResult(false));
        QVERIFY(weak && weak->isInitialized() && !weak->isActive());
        QCOMPARE(active.count(), 2);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(!weak);
    }
    void results_data() {
        QTest::addColumn<int>("decision");
        QTest::newRow("接受") << 1;
        QTest::newRow("取消") << 0;
        QTest::newRow("无决定") << -1;
    }
    void results() {
        QFETCH(int, decision);
        WindowManager manager;
        DialogHostState host;
        connectHost(host, manager);
        QObject requester;
        QStringList events;
        auto vm = std::make_unique<DialogProbe>();
        vm->events = &events;
        QPointer<DialogProbe> weak = vm.get();
        QSignalSpy destroyed(vm.get(), &QObject::destroyed);
        auto future = manager.showDialogAsync(std::move(vm), &requester);
        QVERIFY(manager.busy());
        QCOMPARE(manager.currentDialog(), weak.data());
        QCOMPARE(weak->parent(), &manager);
        QVERIFY(!weak->parentViewModel());
        QVERIFY(weak->isInitialized() && weak->isActive());
        DialogResult result = decision < 0 ? DialogResult{} : DialogResult(decision == 1);
        manager.closeDialog(weak, result);
        manager.closeDialog(weak, !result.value_or(false));
        QVERIFY(future.isFinished());
        QCOMPARE(future.result(), result);
        QVERIFY(!manager.busy());
        QVERIFY(!manager.currentDialog());
        QCOMPARE(events, QStringList({"activate", "close"}));
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(!weak);
        QCOMPARE(destroyed.count(), 1);
    }
    void rejectsAndKeepsCurrent() {
        WindowManager manager;
        QObject requester;
        auto absent = manager.showDialogAsync(std::make_unique<DialogProbe>(), &requester);
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, absent.result());
        DialogHostState host;
        connectHost(host, manager);
        auto invalid = manager.showDialogAsync(nullptr, &requester);
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, invalid.result());
        auto unmapped = manager.showDialogAsync(std::make_unique<UnmappedDialog>(), &requester);
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, unmapped.result());
        auto first = manager.showDialogAsync(std::make_unique<DialogProbe>(), &requester);
        auto *current = manager.currentDialog();
        auto busy = manager.showDialogAsync(std::make_unique<DialogProbe>(), &requester);
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, busy.result());
        QCOMPARE(manager.currentDialog(), current);
        auto active = std::make_unique<DialogProbe>(); active->activate();
        auto refused = manager.showDialogAsync(std::move(active), &requester);
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, refused.result());
        manager.cancelDialogsFor(&requester);
        QCOMPARE(first.result(), DialogResult{});
    }
    void unavailableAndDuplicateHosts() {
        WindowManager manager;
        DialogHostState host; host.setManager(&manager);
        QObject requester;
        auto absent = manager.showDialogAsync(std::make_unique<DialogProbe>(), &requester);
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, absent.result());
        host.setAvailable(true);
        DialogHostState duplicate;
        QTest::ignoreMessage(QtWarningMsg, "DialogHost：窗口服务不受支持或已有宿主");
        duplicate.setManager(&manager); QVERIFY(!duplicate.manager());
        connect(&host, &DialogHostState::hideRequested, &host,
                [&host](const QString &id) { host.released(id); });
        auto future = manager.showDialogAsync(std::make_unique<DialogProbe>(), &requester);
        host.setAvailable(false);
        QCOMPARE(future.result(), DialogResult{}); QVERIFY(!manager.busy());
    }
    void staleNotificationsAndClosingBusy() {
        WindowManager manager;
        DialogHostState host; host.setAvailable(true); host.setManager(&manager);
        QObject requester;
        auto first = manager.showDialogAsync(std::make_unique<DialogProbe>(), &requester);
        const auto oldId = host.requestId();
        manager.closeDialog(manager.currentDialog(), true);
        QVERIFY(manager.busy());
        QVERIFY(!first.isFinished());
        QVERIFY(!manager.currentDialog());
        host.released(oldId);
        QCOMPARE(first.result(), DialogResult(true));
        auto next = manager.showDialogAsync(std::make_unique<DialogProbe>(), &requester);
        host.failed(oldId, "旧失败"); host.dismiss(oldId); host.released(oldId);
        QVERIFY(!next.isFinished());
        const auto nextId = host.requestId();
        host.failed(nextId, "加载失败"); host.released(nextId);
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, next.result());
    }
    void invalidation_data() {
        QTest::addColumn<int>("target");
        QTest::newRow("请求者") << 0;
        QTest::newRow("VM") << 1;
        QTest::newRow("宿主") << 2;
        QTest::newRow("管理者") << 3;
    }
    void invalidation() {
        QFETCH(int, target);
        auto manager = std::make_unique<WindowManager>();
        auto host = std::make_unique<DialogHostState>();
        connectHost(*host, *manager);
        auto requester = std::make_unique<QObject>();
        auto future = manager->showDialogAsync(std::make_unique<DialogProbe>(), requester.get());
        QPointer<ScreenViewModel> weak = manager->currentDialog();
        if (target == 0) requester.reset();
        if (target == 1) delete weak.data();
        if (target == 2) host.reset();
        if (target == 3) manager.reset();
        QTRY_VERIFY(future.isFinished());
        QCOMPARE(future.result(), DialogResult{});
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(!weak);
    }
    void lifecycleExceptions() {
        WindowManager manager; DialogHostState host; connectHost(host, manager);
        QObject requester;
        auto vm = std::make_unique<DialogProbe>(); vm->failActivate = true;
        QPointer<DialogProbe> failedActivation = vm.get();
        QStringList events;
        vm->events = &events;
        auto first = manager.showDialogAsync(std::move(vm), &requester);
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, first.result());
        QVERIFY(!manager.busy());
        QVERIFY(failedActivation && failedActivation->isInitialized() && !failedActivation->isActive());
        QCOMPARE(events, QStringList({"activate", "close"}));
        vm = std::make_unique<DialogProbe>(); vm->failClose = true;
        QPointer<DialogProbe> failedClose = vm.get();
        events.clear(); vm->events = &events;
        auto second = manager.showDialogAsync(std::move(vm), &requester);
        manager.closeDialog(manager.currentDialog(), true);
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, second.result());
        QVERIFY(!manager.busy());
        QVERIFY(failedClose && failedClose->isInitialized() && !failedClose->isActive());
        QCOMPARE(events, QStringList({"activate", "close"}));
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(!failedActivation && !failedClose);
    }
    void invalidCandidatesAreConsumed() {
        WindowManager manager; DialogHostState host; connectHost(host, manager);
        QObject requester, owner;
        auto vm = std::make_unique<DialogProbe>();
        QPointer<DialogProbe> weak = vm.get(); vm->setParent(&owner);
        auto parented = manager.showDialogAsync(std::move(vm), &requester);
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, parented.result());
        QVERIFY(!weak); QVERIFY(owner.children().isEmpty());
        auto absentRequester = manager.showDialogAsync(std::make_unique<DialogProbe>(), nullptr);
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, absentRequester.result());
        QThread worker; worker.start();
        vm = std::make_unique<DialogProbe>(); weak = vm.get(); vm->moveToThread(&worker);
        auto wrongThread = manager.showDialogAsync(std::move(vm), &requester);
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, wrongThread.result());
        QTRY_VERIFY(!weak); worker.quit(); QVERIFY(worker.wait(3000));
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
        auto manager = std::make_shared<WindowManager>();
        DialogHostState host; connectHost(host, *manager);
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
QTEST_GUILESS_MAIN(WindowManagerTests)
#include "tst_windowmanager.moc"
