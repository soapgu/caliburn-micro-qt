#include <CaliburnMicroQt/Conductor.h>
#include <CaliburnMicroQt/ScreenExtensions.h>
#include <QtTest>
#include <stdexcept>

class ActivateOnlyVm : public QObject, public IActivate
{
    Q_OBJECT
    Q_INTERFACES(IActivate)
public:
    bool isActive() const override { return active; }
    void activate() override { active = true; ++activations; }
    bool active = false;
    int activations = 0;
};

class DeactivateOnlyVm : public QObject, public IDeactivate
{
    Q_OBJECT
    Q_INTERFACES(IDeactivate)
public:
    void deactivate(bool close) override {
        emit attempting(close); closes.append(close); emit completed(close);
    }
    QMetaObject::Connection subscribeAttemptingDeactivation(QObject *context, DeactivationCallback callback) override
    { return connect(this, &DeactivateOnlyVm::attempting, context, std::move(callback)); }
    QMetaObject::Connection subscribeDeactivated(QObject *context, DeactivationCallback callback) override
    { return connect(this, &DeactivateOnlyVm::completed, context, std::move(callback)); }
    QList<bool> closes;
signals:
    void attempting(bool close);
    void completed(bool close);
};

// 通过继承获得激活接口，自己声明停用接口；不依赖 Screen。
class LifecycleVm : public ActivateOnlyVm, public IDeactivate
{
    Q_OBJECT
    Q_INTERFACES(IDeactivate)
public:
    void deactivate(bool close) override {
        emit attempting(close); active = false; closes.append(close); emit completed(close);
    }
    QMetaObject::Connection subscribeAttemptingDeactivation(QObject *context, DeactivationCallback callback) override
    { return connect(this, &LifecycleVm::attempting, context, std::move(callback)); }
    QMetaObject::Connection subscribeDeactivated(QObject *context, DeactivationCallback callback) override
    { return connect(this, &LifecycleVm::completed, context, std::move(callback)); }
    QList<bool> closes;
signals:
    void attempting(bool close);
    void completed(bool close);
};

class LifecycleTests : public QObject
{
    Q_OBJECT

    template<class C>
    void checkParentLifecycle()
    {
        C conductor;
        auto item = std::make_unique<LifecycleVm>();
        auto *child = item.get();
        conductor.activateItem(std::move(item));
        QVERIFY(!qobject_cast<ScreenViewModel *>(child));
        QCOMPARE(child->activations, 0);
        conductor.activate();
        QCOMPARE(child->activations, 1);
        conductor.activateItem(child);
        QCOMPARE(child->activations, 2); // 辅助函数不额外判断活动状态。
        conductor.deactivate();
        QCOMPARE(child->closes, QList<bool>({false}));
        QCOMPARE(conductor.activeItem(), child);
        conductor.activate();
        QCOMPARE(child->activations, 3);
        QPointer<LifecycleVm> weak = child;
        conductor.deactivate(true);
        QCOMPARE(child->closes, QList<bool>({false, true}));
        QCOMPARE(conductor.activeItem(), child);
        QVERIFY(weak);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(weak);
    }

    template<class C>
    void checkSeparateCapabilities()
    {
        C conductor;
        conductor.activate();
        auto activatable = std::make_unique<ActivateOnlyVm>();
        auto *first = activatable.get();
        conductor.activateItem(std::move(activatable));
        QCOMPARE(first->activations, 1);
        conductor.closeItem(first); // 缺少停用接口仍可关闭和回收。
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

        auto deactivatable = std::make_unique<DeactivateOnlyVm>();
        auto *second = deactivatable.get();
        conductor.activateItem(std::move(deactivatable));
        conductor.deactivate();
        QCOMPARE(second->closes, QList<bool>({false}));
        conductor.activate();
        conductor.closeItem(second);
        QCOMPARE(second->closes, QList<bool>({false, true}));
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

    template<class C>
    void checkActiveCandidate(const char *warning)
    {
        C conductor;
        auto item = std::make_unique<ActivateOnlyVm>();
        item->activate();
        auto *candidate = item.get();
        QSignalSpy processed(&conductor, &ConductorBase::activationProcessed);
        QTest::ignoreMessage(QtWarningMsg, warning);
        conductor.activateItem(std::move(item));
        QCOMPARE(item.get(), candidate);
        QVERIFY(!candidate->parent());
        QVERIFY(!conductor.activeItem());
        QCOMPARE(processed.count(), 1);
        QCOMPARE(processed.first().at(1).toBool(), false);
    }

    template<class C>
    void checkActiveItemSetter()
    {
        C conductor;
        conductor.activate();
        conductor.activateItem(std::make_unique<LifecycleVm>());
        auto *first = conductor.activeItem();
        conductor.activateItem(std::make_unique<LifecycleVm>());
        QVERIFY(conductor.setProperty("activeItem", QVariant::fromValue(first)));
        QCOMPARE(conductor.activeItem(), first);
        QVERIFY(qobject_cast<IActivate *>(first)->isActive());
        ViewModelBase foreign;
        QSignalSpy processed(&conductor, &ConductorBase::activationProcessed);
        conductor.setActiveItem(&foreign);
        QCOMPARE(conductor.activeItem(), first);
        QCOMPARE(processed.count(), 1);
        QCOMPARE(processed.first().at(1).toBool(), false);
    }

private slots:
    void singleActiveItemSetter() { checkActiveItemSetter<Conductor<>>(); }
    void collectionActiveItemSetter() { checkActiveItemSetter<Conductor<>::Collection::OneActive>(); }
    void sharedBaseAndEnsureItemOrder()
    {
        class Probe : public Conductor<> {
        public:
            std::function<void()> duringEnsure;
            QObject *ensureItem(QObject *item) override {
                if (duringEnsure) duringEnsure();
                return ConductorBase::ensureItem(item);
            }
        } conductor;
        static_assert(std::is_abstract_v<ConductorBaseWithActiveItem>);
        QCOMPARE(ConductorSingleItem::staticMetaObject.superClass(), &ConductorBaseWithActiveItem::staticMetaObject);
        QCOMPARE(ConductorCollectionOneActiveBase::staticMetaObject.superClass(), &ConductorBaseWithActiveItem::staticMetaObject);
        auto strategy = conductor.closeStrategy();
        QCOMPARE(conductor.closeStrategy(), strategy);
        conductor.setCloseStrategy(nullptr);
        QVERIFY(conductor.closeStrategy() != strategy);
        conductor.activate();
        conductor.activateItem(std::make_unique<LifecycleVm>());
        auto *old = static_cast<LifecycleVm *>(conductor.activeItem());
        conductor.duringEnsure = [&] {
            QCOMPARE(conductor.activeItem(), old);
            QCOMPARE(old->closes, QList<bool>{true});
            QVERIFY(!old->isActive());
        };
        auto next = std::make_unique<LifecycleVm>();
        auto *raw = next.get();
        connect(&conductor, &ConductorBaseWithActiveItem::activeItemChanged, &conductor, [&] {
            QCOMPARE(conductor.activeItem(), raw);
            QVERIFY(raw->isActive());
        });
        conductor.activateItem(std::move(next));
    }

    void failedActivationKeepsPreviousSelection()
    {
        class Throwing : public ScreenViewModel {
            void onActivate() override { throw std::runtime_error("激活失败"); }
        };
        Conductor<> conductor;
        conductor.activate();
        conductor.activateItem(std::make_unique<LifecycleVm>());
        auto *old = conductor.activeItem();
        QSignalSpy changed(&conductor, &ConductorBaseWithActiveItem::activeItemChanged);
        auto next = std::make_unique<Throwing>();
        auto *raw = next.get();
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, conductor.activateItem(std::move(next)));
        QCOMPARE(conductor.activeItem(), old);
        QCOMPARE(changed.count(), 0);
        QVERIFY(!qobject_cast<IActivate *>(old)->isActive());
        QVERIFY(raw->isActive()); // Screen 已提交状态，但公共基类未提交选择。
        QCOMPARE(raw->parentViewModel(), &conductor);
    }

    void optionalCapabilities()
    {
        QObject plain;
        ScreenExtensions::tryActivate(nullptr);
        ScreenExtensions::tryDeactivate(nullptr, true);
        ScreenExtensions::tryActivate(&plain);
        ScreenExtensions::tryDeactivate(&plain, false);
        ActivateOnlyVm activatable;
        ScreenExtensions::tryActivate(&activatable);
        ScreenExtensions::tryDeactivate(&activatable, true);
        QCOMPARE(activatable.activations, 1);
        DeactivateOnlyVm deactivatable;
        ScreenExtensions::tryActivate(&deactivatable);
        ScreenExtensions::tryDeactivate(&deactivatable, false);
        ScreenExtensions::tryDeactivate(&deactivatable, true);
        QCOMPARE(deactivatable.closes, QList<bool>({false, true}));
    }

    void screenImplementsInterfaces()
    {
        ScreenViewModel screen;
        QCOMPARE(qobject_cast<IActivate *>(&screen), static_cast<IActivate *>(&screen));
        QCOMPARE(qobject_cast<IDeactivate *>(&screen), static_cast<IDeactivate *>(&screen));
        ScreenExtensions::tryActivate(&screen);
        QVERIFY(screen.isInitialized() && screen.isActive());
        ScreenExtensions::tryDeactivate(&screen, false);
        QVERIFY(!screen.isActive());
        Conductor<> conductor;
        QVERIFY(qobject_cast<IActivate *>(&conductor));
        QVERIFY(qobject_cast<IDeactivate *>(&conductor));
    }

    void singleParentLifecycle() { checkParentLifecycle<Conductor<>>(); }
    void collectionParentLifecycle() { checkParentLifecycle<Conductor<>::Collection::OneActive>(); }
    void singleSeparateCapabilities() { checkSeparateCapabilities<Conductor<>>(); }
    void collectionSeparateCapabilities() { checkSeparateCapabilities<Conductor<>::Collection::OneActive>(); }

    void singleSwitch()
    {
        Conductor<> conductor;
        conductor.activate();
        auto a = std::make_unique<LifecycleVm>();
        auto *first = a.get();
        conductor.activateItem(std::move(a));
        auto b = std::make_unique<LifecycleVm>();
        auto *second = b.get();
        conductor.activateItem(std::move(b));
        QCOMPARE(first->closes, QList<bool>({true}));
        QCOMPARE(second->activations, 1);
        conductor.deactivateItem(second, false);
        QCOMPARE(second->closes, QList<bool>({false}));
        QVERIFY(!conductor.activeItem());
        conductor.activateItem(second);
        QCOMPARE(second->activations, 2);
    }

    void collectionSwitchAndClose()
    {
        Conductor<>::Collection::OneActive conductor;
        conductor.activate();
        auto a = std::make_unique<LifecycleVm>();
        auto *first = a.get();
        conductor.activateItem(std::move(a));
        auto b = std::make_unique<LifecycleVm>();
        auto *second = b.get();
        QVERIFY(conductor.addItem(std::move(b)));
        QCOMPARE(second->activations, 0);
        conductor.activateItem(second);
        QCOMPARE(first->closes, QList<bool>({false}));
        QCOMPARE(second->activations, 1);
        conductor.deactivateItem(second, false);
        QCOMPARE(conductor.activeItem(), second);
        conductor.activateItem(second);
        QCOMPARE(second->activations, 2);
        conductor.closeItem(second);
        QCOMPARE(second->closes, QList<bool>({false, true}));
        QCOMPARE(conductor.activeItem(), first);
        QCOMPARE(first->activations, 2);
        conductor.activateItem(std::make_unique<LifecycleVm>());
        conductor.deactivate(true); // 父关闭同时分派给活动和非活动成员。
        QCOMPARE(first->closes, QList<bool>({false, false, true}));
        QVERIFY(conductor.items().isEmpty());
    }

    void rejectsActiveCandidates()
    {
        checkActiveCandidate<Conductor<>>("Conductor: cannot adopt an active item");
        checkActiveCandidate<Conductor<>::Collection::OneActive>("Collection.OneActive: cannot adopt an active item");
        Conductor<>::Collection::OneActive collection;
        auto item = std::make_unique<ActivateOnlyVm>();
        item->activate();
        QTest::ignoreMessage(QtWarningMsg, "Collection.OneActive: cannot adopt an active item");
        QVERIFY(!collection.addItem(std::move(item)));
        QVERIFY(item);
        QVERIFY(collection.items().isEmpty());
    }
};

QTEST_GUILESS_MAIN(LifecycleTests)
#include "tst_lifecycle.moc"
