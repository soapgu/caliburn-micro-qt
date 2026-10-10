#include <CaliburnMicroQt/Conductor.h>
#include <CaliburnMicroQt/DefaultCloseStrategy.h>
#include <QtTest>
#include <stdexcept>

class GuardScreen : public ScreenViewModel
{
public:
    bool deferred = false;
    bool answer = true;
    bool throws = false;
    int checks = 0;
    int closes = 0;
    int *destroyed = nullptr;
    QList<CloseCallback> callbacks;
    ~GuardScreen() override { if (destroyed) ++*destroyed; }
    void canClose(CloseCallback callback) override
    {
        ++checks;
        if (throws) throw std::runtime_error("守卫异常");
        if (deferred) callbacks.append(std::move(callback));
        else callback(answer);
    }
protected:
    void onDeactivate(bool close) override { if (close) ++closes; }
};

class GuardPlain : public ViewModelBase, public IGuardClose
{
    Q_OBJECT
    Q_INTERFACES(IGuardClose)
public:
    int checks = 0;
    void canClose(CloseCallback callback) override { ++checks; callback(false); }
};

class DeferredStrategy : public ICloseStrategy
{
public:
    CloseCallback result;
    void execute(const QList<ViewModelBase *> &, CloseCallback callback) override { result = std::move(callback); }
};

class GuardCloseTests : public QObject
{
    Q_OBJECT
private slots:
    void defaultScreenAndLifecycleSignals()
    {
        ScreenViewModel screen;
        QVERIFY(qobject_cast<IGuardClose *>(&screen));
        bool allowed = false;
        screen.canClose([&](bool value) { allowed = value; });
        QVERIFY(allowed);
        QStringList order;
        connect(&screen, &ScreenViewModel::attemptingDeactivation, &screen, [&](bool close) {
            QVERIFY(close && screen.isActive()); order << "before";
        });
        connect(&screen, &ScreenViewModel::isActiveChanged, &screen, [&] { if (!screen.isActive()) order << "state"; });
        connect(&screen, &ScreenViewModel::deactivated, &screen, [&](bool close) {
            QVERIFY(close && !screen.isActive()); order << "after";
        });
        screen.activate(); screen.deactivate(true);
        QCOMPARE(order, QStringList({"before", "state", "after"}));
        screen.tryClose(); // 根对象不触发关闭请求。
        QCOMPARE(order.size(), 3);
    }

    void strategyContinuesAfterRejection()
    {
        GuardScreen a, b, c;
        a.answer = false; b.deferred = true;
        DefaultCloseStrategy strategy;
        int completions = 0;
        bool allowed = true;
        strategy.execute({&a, &b, &c}, [&](bool value) { ++completions; allowed = value; });
        QCOMPARE(a.checks, 1); QCOMPARE(b.checks, 1); QCOMPARE(c.checks, 0);
        QCOMPARE(completions, 0);
        auto finish = b.callbacks.first(); finish(true);
        QCOMPARE(c.checks, 1); QCOMPARE(completions, 1); QVERIFY(!allowed);
    }

    void guardAndStrategyExceptionsPropagate()
    {
        GuardScreen a, b; a.throws = true;
        DefaultCloseStrategy strategy;
        bool completed = false;
        QVERIFY_THROWS_EXCEPTION(std::runtime_error,
            strategy.execute({&a, &b}, [&](bool) { completed = true; }));
        QVERIFY(!completed); QCOMPARE(b.checks, 0);
        Conductor<GuardScreen> c;
        c.activateItem(std::make_unique<GuardScreen>());
        auto *current = c.activeItem(); current->throws = true;
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, c.closeItem(current));
        QCOMPARE(c.activeItem(), current);
        int destroyed = 0;
        auto candidate = std::make_unique<GuardScreen>(); candidate->destroyed = &destroyed;
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, c.activateItem(std::move(candidate)));
        QVERIFY(!candidate); QCOMPARE(destroyed, 1); QCOMPARE(c.activeItem(), current);
    }

    void destroyedSnapshotMemberIsRejected()
    {
        GuardScreen first, last; first.deferred = true;
        auto next = std::make_unique<GuardScreen>();
        DefaultCloseStrategy strategy;
        bool allowed = true;
        strategy.execute({&first, next.get(), &last}, [&](bool result) { allowed = result; });
        next.reset(); first.callbacks.first()(true);
        QVERIFY(!allowed); QCOMPARE(last.checks, 1);
    }

    void singleReplacement_data()
    {
        QTest::addColumn<bool>("deferred"); QTest::addColumn<bool>("allowed");
        for (bool deferred : {false, true}) for (bool allowed : {false, true})
            QTest::newRow(qPrintable(QString("%1-%2").arg(deferred).arg(allowed))) << deferred << allowed;
    }
    void singleReplacement()
    {
        QFETCH(bool, deferred); QFETCH(bool, allowed);
        Conductor<GuardScreen> c; c.activate();
        auto owned = std::make_unique<GuardScreen>(); auto *old = owned.get();
        c.activateItem(std::move(owned)); old->deferred = deferred; old->answer = allowed;
        int destroyed = 0;
        auto next = std::make_unique<GuardScreen>(); next->destroyed = &destroyed;
        QPointer<GuardScreen> candidate = next.get();
        QSignalSpy completed(&c, &ConductorBase::activationProcessed);
        c.activateItem(std::move(next)); QVERIFY(!next);
        if (deferred) {
            QCOMPARE(c.activeItem(), old); QVERIFY(candidate && !candidate->parent());
            QVERIFY(!candidate->parentViewModel()); QCOMPARE(old->closes, 0);
            auto finish = old->callbacks.first(); finish(allowed);
        }
        QCOMPARE(completed.count(), 1);
        QCOMPARE(completed.first().at(1).toBool(), allowed);
        if (allowed) { QCOMPARE(c.activeItem(), candidate.data()); QCOMPARE(old->closes, 1); QCOMPARE(destroyed, 0); }
        else { QCOMPARE(c.activeItem(), old); QVERIFY(!candidate); QCOMPARE(destroyed, 1); }
        // 指针计数器必须晚于 Conductor 回收其成员。
        c.deactivate(true); QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

    void singleAndCollectionDeactivationDiffer()
    {
        Conductor<GuardScreen> single;
        auto a = std::make_unique<GuardScreen>(); auto *s = a.get(); single.activateItem(std::move(a)); single.activate();
        s->answer = false;
        single.deactivateItem(s, false); QCOMPARE(single.activeItem(), s); QVERIFY(s->isActive()); QCOMPARE(s->checks, 1);
        single.deactivate(false); QVERIFY(!s->isActive()); QCOMPARE(s->checks, 1);
        Conductor<GuardScreen>::Collection::OneActive collection;
        auto b = std::make_unique<GuardScreen>(); auto *t = b.get(); collection.activateItem(std::move(b)); collection.activate();
        t->answer = false;
        collection.deactivateItem(t, false); QVERIFY(!t->isActive()); QCOMPARE(t->checks, 0); QCOMPARE(collection.activeItem(), t);
        collection.activateItem(std::make_unique<GuardScreen>()); QCOMPARE(t->checks, 0);
        collection.closeItem(t); QCOMPARE(t->checks, 1); QCOMPARE(collection.items().size(), 2);
    }

    void parentChecksOnlyCurrentAndNestedMembers()
    {
        Conductor<ScreenViewModel> outer;
        auto nested = std::make_unique<Conductor<GuardScreen>>(); auto *inner = nested.get();
        auto a = std::make_unique<GuardScreen>(); auto *old = a.get(); inner->activateItem(std::move(a));
        inner->deactivateItem(old, false); old->answer = false;
        auto b = std::make_unique<GuardScreen>(); auto *current = b.get(); inner->activateItem(std::move(b));
        outer.activateItem(std::move(nested)); outer.activate();
        bool allowed = false; outer.canClose([&](bool result) { allowed = result; });
        QVERIFY(allowed); QCOMPARE(old->checks, 1); QCOMPARE(current->checks, 1);
        QCOMPARE(inner->getChildren(), QList<ViewModelBase *>{current});
        QCOMPARE(old->parentViewModel(), inner);
        current->answer = false; outer.canClose([&](bool result) { allowed = result; });
        QVERIFY(!allowed); QCOMPARE(current->closes, 0);
        current->answer = true; outer.closeItem(inner); QVERIFY(!outer.activeItem());
        QCOMPARE(current->closes, 1); QCOMPARE(old->closes, 0);
    }

    void collectionParentChecksAllMembersWithoutClosing()
    {
        Conductor<GuardScreen>::Collection::OneActive c;
        c.activateItem(std::make_unique<GuardScreen>()); auto *a = c.activeItem();
        c.activateItem(std::make_unique<GuardScreen>()); auto *b = c.activeItem();
        a->answer = false;
        bool allowed = true; c.canClose([&](bool result) { allowed = result; });
        QVERIFY(!allowed); QCOMPARE(a->checks, 1); QCOMPARE(b->checks, 1);
        QCOMPARE(c.items().size(), 2); QCOMPARE(c.activeItem(), b);
        QCOMPARE(a->closes, 0); QCOMPARE(b->closes, 0);
    }

    void deferredCallbackAfterOwnerDestructionIsSafe()
    {
        auto c = std::make_unique<Conductor<GuardScreen>>();
        auto strategy = std::make_shared<DeferredStrategy>(); c->setCloseStrategy(strategy);
        int destroyed = 0;
        auto next = std::make_unique<GuardScreen>(); next->destroyed = &destroyed;
        c->activateItem(std::move(next)); c.reset();
        QCOMPARE(destroyed, 0); // 候选由回调持有，管理者不再主动取消。
        strategy->result(true); QCOMPARE(destroyed, 1);
    }

    void releasedCallbackReclaimsCandidate()
    {
        int destroyed = 0;
        Conductor<GuardScreen> c;
        auto strategy = std::make_shared<DeferredStrategy>(); c.setCloseStrategy(strategy);
        auto candidate = std::make_unique<GuardScreen>(); candidate->destroyed = &destroyed;
        c.activateItem(std::move(candidate)); QCOMPARE(destroyed, 0);
        strategy->result = {}; QCOMPARE(destroyed, 1); QVERIFY(!c.activeItem());
    }

    void plainVmCanOptIntoGuard()
    {
        Conductor<> c;
        auto candidate = std::make_unique<GuardPlain>(); auto *plain = candidate.get();
        c.activateItem(std::move(candidate)); c.closeItem(plain);
        QCOMPARE(plain->checks, 1); QCOMPARE(c.activeItem(), plain);
    }

    void strategyExceptionAfterPermissionDoesNotRollback()
    {
        class ThrowAfterStrategy : public ICloseStrategy {
            void execute(const QList<ViewModelBase *> &, CloseCallback callback) override {
                callback(true); throw std::runtime_error("策略许可后异常");
            }
        };
        Conductor<GuardScreen> c; c.activate();
        c.activateItem(std::make_unique<GuardScreen>());
        QPointer<GuardScreen> old = c.activeItem();
        c.setCloseStrategy(std::make_shared<ThrowAfterStrategy>());
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, c.closeItem(old));
        QVERIFY(!c.activeItem()); QCOMPARE(old->closes, 1);
    }

    void synchronousAndDeferredLifecycleExceptions()
    {
        class Throwing : public ScreenViewModel { void onActivate() override { throw std::runtime_error("激活失败"); } };
        Conductor<ScreenViewModel> c; c.activate();
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, c.activateItem(std::make_unique<Throwing>()));
        QVERIFY(c.activeItem() && c.activeItem()->isActive());
        auto strategy = std::make_shared<DeferredStrategy>(); c.setCloseStrategy(strategy);
        c.activateItem(std::make_unique<Throwing>());
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, strategy->result(true));
        QVERIFY(c.activeItem() && c.activeItem()->isActive());
    }
};

QTEST_GUILESS_MAIN(GuardCloseTests)
#include "tst_guardclose.moc"
