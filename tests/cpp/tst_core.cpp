#include <CaliburnMicroQt/ViewModelBase.h>
#include <HomeViewModel.h>
#include <ShellViewModel.h>
#include <CaliburnMicroQt/ViewRegistry.h>
#include <QtTest>
#include <type_traits>
#include <memory>
#include <limits>
#include <QThread>
#include <stdexcept>

struct TrackedValue
{
    int value = 0;
    int assignments = 0;
    bool operator==(const TrackedValue &other) const { return value == other.value; }
    TrackedValue &operator=(const TrackedValue &other)
    {
        value = other.value;
        ++assignments;
        return *this;
    }
};

class NotifyVm : public ViewModelBase
{
    Q_OBJECT
public:
    using ViewModelBase::ViewModelBase;
    using ViewModelBase::setAndNotify;
    int value = 0;
    QString text;
    TrackedValue tracked;
signals:
    void changed();
};

class DerivedHome : public HomeViewModel { Q_OBJECT };

class LifecycleHome : public HomeViewModel
{
public:
    QStringList events;
protected:
    void onInitialize() override { events << "initialize"; }
    void onActivate() override { events << "activate"; }
    void onDeactivate(bool close) override { events << (close ? "close" : "deactivate"); }
};

class ProbeScreen : public ScreenViewModel
{
public:
    QStringList events;
    bool reenter = false;
protected:
    void onInitialize() override
    {
        events << QStringLiteral("initialize:%1:%2").arg(isInitialized()).arg(isActive());
        if (reenter)
            activate();
    }
    void onActivate() override
    {
        events << QStringLiteral("activate:%1:%2").arg(isInitialized()).arg(isActive());
    }
    void onDeactivate(bool close) override
    {
        events << QStringLiteral("deactivate:%1:%2").arg(close).arg(isActive());
    }
};

// 通知辅助只接受无参数 void 成员指针，QObject 基类不可复制或移动。
using NotifyHelper = bool (ViewModelBase::*)(int &, const int &, void (NotifyVm::*)());
static_assert(std::is_same_v<decltype(&NotifyVm::setAndNotify<NotifyVm, int>), NotifyHelper>);
static_assert(!std::is_invocable_v<NotifyHelper, NotifyVm *, int &, const int &,
                                  void (NotifyVm::*)(int)>);
static_assert(!std::is_invocable_v<NotifyHelper, NotifyVm *, int &, const int &,
                                  int (NotifyVm::*)()>);
static_assert(!std::is_copy_constructible_v<ViewModelBase>);
static_assert(!std::is_move_constructible_v<ViewModelBase>);

class CoreTests : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QVERIFY(ViewRegistry::viewUrl(nullptr).isEmpty());
        for (const QUrl &url : {QUrl(), QUrl(QStringLiteral("views/Home.qml")),
                               QUrl(QStringLiteral("http://["))}) {
            QTest::ignoreMessage(QtWarningMsg, "ViewRegistry：View URL 必须为有效、非空的绝对地址");
            QVERIFY(!ViewRegistry::registerView<HomeViewModel>(url));
        }
        const QUrl home(QStringLiteral("qrc:/qt/qml/CaliburnExample/views/HomeView.qml"));
        QVERIFY(ViewRegistry::registerView<HomeViewModel>(home));
        QVERIFY(ViewRegistry::registerView<HomeViewModel>(home));
        QTest::ignoreMessage(QtWarningMsg, "ViewRegistry：类型映射冲突 HomeViewModel");
        QVERIFY(!ViewRegistry::registerView<HomeViewModel>(QUrl(QStringLiteral("qrc:/different.qml"))));
        QVERIFY(ViewRegistry::registerView<ShellViewModel>(QUrl(QStringLiteral("qrc:/qt/qml/CaliburnExample/views/ShellView.qml"))));
    }

    void screenLifecycle()
    {
        ProbeScreen screen;
        QPointer<ScreenViewModel> weak = &screen;
        QSignalSpy initialized(&screen, &ScreenViewModel::isInitializedChanged);
        QSignalSpy active(&screen, &ScreenViewModel::isActiveChanged);
        connect(&screen, &ScreenViewModel::isInitializedChanged, this, [&] {
            screen.events << QStringLiteral("initialized:%1").arg(screen.isInitialized());
        });
        connect(&screen, &ScreenViewModel::isActiveChanged, this, [&] {
            screen.events << QStringLiteral("active:%1").arg(screen.isActive());
        });
        screen.deactivate();
        screen.deactivate(true);
        QVERIFY(screen.events.isEmpty());
        screen.activate();
        QCOMPARE(screen.events, QStringList({"initialize:0:0", "initialized:1", "activate:1:0", "active:1"}));
        screen.initialize();
        screen.activate();
        QCOMPARE(initialized.count(), 1);
        QCOMPARE(active.count(), 1);
        screen.deactivate();
        screen.deactivate();
        screen.deactivate(true);
        screen.deactivate(true);
        QCOMPARE(screen.events.mid(4), QStringList({"deactivate:0:1", "active:0", "deactivate:1:0"}));
        QCOMPARE(active.count(), 2);
        QVERIFY(weak);
        QVERIFY(screen.isInitialized());
        screen.activate();
        screen.deactivate(true);
        screen.deactivate(true);
        QCOMPARE(screen.events.mid(7), QStringList({"activate:1:0", "active:1", "deactivate:1:1", "active:0"}));
        QCOMPARE(initialized.count(), 1);
        QCOMPARE(active.count(), 4);
        QVERIFY(!screen.isActive());

        ProbeScreen neverActive;
        neverActive.initialize();
        neverActive.initialize();
        neverActive.deactivate(true);
        neverActive.deactivate(true);
        QCOMPARE(neverActive.events, QStringList({"initialize:0:0", "deactivate:1:0"}));
        neverActive.activate();
        QVERIFY(neverActive.isActive());
    }

    void screenRejectsReentryAndWorker()
    {
        ProbeScreen screen;
        screen.reenter = true;
        QTest::ignoreMessage(QtWarningMsg, "ScreenViewModel：拒绝生命周期重入");
        screen.initialize();
        QVERIFY(screen.isInitialized());
        QVERIFY(!screen.isActive());
        connect(&screen, &ScreenViewModel::isActiveChanged, this, [&] { screen.deactivate(); });
        QTest::ignoreMessage(QtWarningMsg, "ScreenViewModel：拒绝生命周期重入");
        screen.activate();
        QVERIFY(screen.isActive());
        const auto events = screen.events;
        QTest::ignoreMessage(QtWarningMsg, "ScreenViewModel：生命周期必须在应用主线程调用");
        auto worker = std::unique_ptr<QThread>(QThread::create([&] { screen.deactivate(true); }));
        worker->start();
        QVERIFY(worker->wait(5000));
        QCOMPARE(screen.events, events);
        QVERIFY(screen.isActive());
        QVERIFY(screen.metaObject()->indexOfMethod("activate()") < 0);
        QVERIFY(screen.metaObject()->indexOfMethod("initialize()") < 0);
    }

    void registryContract()
    {
        HomeViewModel home;
        ShellViewModel shell(std::make_unique<HomeViewModel>());
        ViewRegistry registry;
        const QUrl homeUrl(QStringLiteral("qrc:/qt/qml/CaliburnExample/views/HomeView.qml"));
        QCOMPARE(ViewRegistry::viewUrl(&home), homeUrl);
        QCOMPARE(registry.resolve(&home), homeUrl);
        QCOMPARE(ViewRegistry::viewUrl(&shell), QUrl(QStringLiteral("qrc:/qt/qml/CaliburnExample/views/ShellView.qml")));
        DerivedHome unknown;
        QTest::ignoreMessage(QtWarningMsg, "ViewRegistry：未登记类型 DerivedHome");
        QVERIFY(ViewRegistry::viewUrl(&unknown).isEmpty());
        QTest::ignoreMessage(QtWarningMsg, "ViewRegistry：登记、冻结与查询必须在应用主线程调用");
        QTest::ignoreMessage(QtWarningMsg, "ViewRegistry：登记、冻结与查询必须在应用主线程调用");
        QUrl workerUrl;
        bool workerFrozen = true;
        auto worker = std::unique_ptr<QThread>(QThread::create([&] {
            workerUrl = ViewRegistry::viewUrl(&home);
            workerFrozen = ViewRegistry::freeze();
        }));
        worker->start();
        QVERIFY(worker->wait(5000));
        QVERIFY(workerUrl.isEmpty());
        QVERIFY(!workerFrozen);
        // 成功、未知类型和非法线程查询都不能结束配置；非法线程冻结也无副作用。
        const QUrl derivedUrl(QStringLiteral("qrc:/derived.qml"));
        QVERIFY(ViewRegistry::registerView<DerivedHome>(derivedUrl));
        QCOMPARE(ViewRegistry::viewUrl(&unknown), derivedUrl);
        QVERIFY(ViewRegistry::freeze());
        QVERIFY(ViewRegistry::freeze());
        QTest::ignoreMessage(QtWarningMsg, "ViewRegistry：配置已冻结，拒绝登记");
        QVERIFY(!ViewRegistry::registerView<HomeViewModel>(homeUrl));
        QCOMPARE(ViewRegistry::viewUrl(&home), homeUrl);
        QCOMPARE(registry.resolve(&home), homeUrl);
    }

    void shellOwnsHomeAfterHandoff()
    {
        auto home = std::make_unique<HomeViewModel>();
        QVERIFY(!home->parent());
        QPointer<HomeViewModel> weak = home.get();
        auto shell = std::make_unique<ShellViewModel>(std::move(home));
        QVERIFY(!home);
        QCOMPARE(shell->home(), weak.data());
        QCOMPARE(shell->home()->parent(), shell.get());
        shell->activate();
        QVERIFY(shell->home()->isInitialized());
        QVERIFY(shell->home()->isActive());
        shell->home()->add(2);
        shell->deactivate(true);
        QVERIFY(!shell->home()->isActive());
        QVERIFY(weak);
        QCOMPARE(shell->home()->count(), 2);
        shell.reset();
        QVERIFY(!weak);

        ShellViewModel owner(std::make_unique<HomeViewModel>());
        QSignalSpy changed(&owner, &ShellViewModel::homeChanged);
        delete owner.home(); // 验证异常提前销毁后，父对象访问引用仍安全清空。
        QVERIFY(!owner.home());
        QCOMPARE(changed.count(), 1);
        owner.activate();
        owner.deactivate(true);
        QVERIFY(owner.metaObject()->indexOfProperty("count") < 0);
        QVERIFY(owner.metaObject()->indexOfMethod("add(int)") < 0);
    }

    void shellRejectsInvalidHandoff()
    {
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, ShellViewModel(std::unique_ptr<HomeViewModel>{}));
        QObject parent;
        auto home = std::make_unique<HomeViewModel>();
        home->setParent(&parent);
        QPointer<HomeViewModel> weak = home.get();
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, ShellViewModel(std::move(home)));
        QVERIFY(!weak);
        QVERIFY(parent.children().isEmpty());
    }

    void shellPropagatesHomeHooks()
    {
        auto candidate = std::make_unique<LifecycleHome>();
        auto *home = candidate.get();
        ShellViewModel shell(std::move(candidate));
        shell.activate();
        shell.initialize();
        shell.activate();
        shell.deactivate();
        shell.deactivate();
        shell.deactivate(true); // 已停用的 Home 仍收到一次关闭钩子。
        shell.deactivate(true);
        QCOMPARE(home->events, QStringList({"initialize", "activate", "deactivate", "close"}));
        shell.activate();
        shell.deactivate(true);
        QCOMPARE(home->events.mid(4), QStringList({"activate", "close"}));
    }

    void baseContract()
    {
        QObject parent;
        ViewModelBase base(&parent);
        QCOMPARE(base.parent(), &parent);
        QCOMPARE(base.metaObject()->propertyCount(), QObject::staticMetaObject.propertyCount());
        QCOMPARE(base.metaObject()->methodCount(), QObject::staticMetaObject.methodCount());
        HomeViewModel home;
        QCOMPARE(qobject_cast<ViewModelBase *>(&home), static_cast<ViewModelBase *>(&home));
    }

    void typedNotifications()
    {
        NotifyVm vm;
        QSignalSpy spy(&vm, &NotifyVm::changed);
        int observed = -1;
        connect(&vm, &NotifyVm::changed, this, [&] { observed = vm.value; });
        QVERIFY(vm.setAndNotify(vm.value, 3, &NotifyVm::changed));
        QCOMPARE(observed, 3);
        QCOMPARE(spy.count(), 1);
        QVERIFY(!vm.setAndNotify(vm.value, 3, &NotifyVm::changed));
        QCOMPARE(spy.count(), 1);
        QVERIFY(vm.setAndNotify(vm.text, QStringLiteral("文字"), &NotifyVm::changed));
        QCOMPARE(vm.text, QStringLiteral("文字"));
        QCOMPARE(spy.count(), 2);
        QVERIFY(!vm.setAndNotify(vm.tracked, TrackedValue{0}, &NotifyVm::changed));
        QCOMPARE(vm.tracked.assignments, 0);
        QVERIFY(vm.setAndNotify(vm.tracked, TrackedValue{2}, &NotifyVm::changed));
        QCOMPARE(vm.tracked.assignments, 1);
        QCOMPARE(spy.count(), 3);
    }

    void parentOwnsChildren()
    {
        auto parent = std::make_unique<ViewModelBase>();
        auto *child = new NotifyVm(parent.get());
        QPointer<NotifyVm> weak = child;
        QSignalSpy destroyed(child, &QObject::destroyed);
        parent.reset();
        QVERIFY(!weak);
        QCOMPARE(destroyed.count(), 1);
    }

    void invalidNotificationDoesNotWrite()
    {
        NotifyVm vm;
        QSignalSpy spy(&vm, &NotifyVm::changed);
        void (NotifyVm::*nullSignal)() = nullptr;
        QTest::ignoreMessage(QtWarningMsg, "ViewModelBase::setAndNotify: 信号为空或对象类型不兼容");
        QVERIFY(!vm.setAndNotify(vm.value, 2, nullSignal));
        QTest::ignoreMessage(QtWarningMsg, "ViewModelBase::setAndNotify: 信号为空或对象类型不兼容");
        QVERIFY(!vm.setAndNotify(vm.value, 2, &HomeViewModel::countChanged));
        QCOMPARE(vm.value, 0);
        QCOMPARE(spy.count(), 0);
    }

    void homeNotificationsAndBoundaries()
    {
        HomeViewModel vm;
        QSignalSpy count(&vm, &HomeViewModel::countChanged);
        QSignalSpy increment(&vm, &HomeViewModel::canIncrementChanged);
        QSignalSpy addTwo(&vm, &HomeViewModel::canAddTwoChanged);
        QSignalSpy reset(&vm, &HomeViewModel::canResetChanged);
        QString observed;
        connect(&vm, &HomeViewModel::countChanged, this, [&] { observed = vm.message(); });
        QCOMPARE(vm.message(), QStringLiteral("已点击 0 次"));
        QCOMPARE(vm.incrementText(), QStringLiteral("增加"));
        vm.reset();
        QCOMPARE(count.count(), 0);
        vm.increment();
        QCOMPARE(observed, QStringLiteral("已点击 1 次"));
        QCOMPARE(count.count(), 1);
        QCOMPARE(increment.count(), 0);
        QCOMPARE(reset.count(), 1);
        for (int i = 0; i < 10; ++i)
            vm.increment();
        QCOMPARE(vm.count(), 5);
        QCOMPARE(count.count(), 5);
        QCOMPARE(increment.count(), 1);
        QVERIFY(!vm.canIncrement());
        QVERIFY(!vm.canAddTwo());
        QCOMPARE(addTwo.count(), 1);
        vm.reset();
        QCOMPARE(vm.count(), 0);
        QCOMPARE(count.count(), 6);
        QCOMPARE(increment.count(), 2);
        QCOMPARE(reset.count(), 2);
        QVERIFY(!vm.canReset());
        QVERIFY(vm.canAddTwo());
        QCOMPARE(addTwo.count(), 2);
    }

    void homeAddBoundaries_data()
    {
        QTest::addColumn<int>("initial");
        QTest::addColumn<int>("delta");
        for (int initial = 0; initial <= 5; ++initial) {
            for (int delta : {std::numeric_limits<int>::min(), -1, 0, 1, 2, 3, 5, 6,
                              std::numeric_limits<int>::max()}) {
                const QByteArray row = QByteArray::number(initial) + "/" + QByteArray::number(delta);
                QTest::newRow(row.constData()) << initial << delta;
            }
        }
    }

    void homeAddBoundaries()
    {
        QFETCH(int, initial);
        QFETCH(int, delta);
        HomeViewModel vm;
        for (int i = 0; i < initial; ++i)
            vm.increment();
        QSignalSpy count(&vm, &HomeViewModel::countChanged);
        QSignalSpy increment(&vm, &HomeViewModel::canIncrementChanged);
        QSignalSpy addTwo(&vm, &HomeViewModel::canAddTwoChanged);
        QSignalSpy reset(&vm, &HomeViewModel::canResetChanged);
        const bool accepted = delta > 0 && delta <= 5 - initial;
        const int expected = accepted ? initial + delta : initial;
        vm.add(delta);
        QCOMPARE(vm.count(), expected);
        QCOMPARE(vm.message(), QStringLiteral("已点击 %1 次").arg(expected));
        QCOMPARE(vm.canIncrement(), expected < 5);
        QCOMPARE(vm.canAddTwo(), expected <= 3);
        QCOMPARE(vm.canReset(), expected > 0);
        QCOMPARE(count.count(), accepted ? 1 : 0);
        QCOMPARE(increment.count(), int((initial < 5) != (expected < 5)));
        QCOMPARE(addTwo.count(), int((initial <= 3) != (expected <= 3)));
        QCOMPARE(reset.count(), int((initial > 0) != (expected > 0)));
    }

};

QTEST_GUILESS_MAIN(CoreTests)
#include "tst_core.moc"
