#include <CaliburnMicroQt/Conductor.h>
#include <QMetaProperty>
#include <QtTest>
#include <stdexcept>

class ProtocolScreen : public ScreenViewModel
{
public:
    QString name;
    QStringList *events = nullptr;
    int initialized = 0;
    int closed = 0;
    int *destroyed = nullptr;
    bool closeSawEmptyParent = true;
    ~ProtocolScreen() override { if (destroyed) ++*destroyed; }
protected:
    void onInitialize() override { ++initialized; }
    void onActivate() override { if (events) *events << name + ":activate"; }
    void onDeactivate(bool close) override
    {
        if (events) *events << name + (close ? ":close" : ":deactivate");
        if (close) {
            ++closed;
            closeSawEmptyParent = closeSawEmptyParent && !parentViewModel();
        }
    }
};

// 普通 VM 可以自行选择接入 IChild，而不需要 Screen 生命周期。
class CustomChild : public ViewModelBase, public IChild
{
    Q_OBJECT
    Q_INTERFACES(IChild)
public:
    explicit CustomChild(QObject *logicalParent = nullptr) : m_logicalParent(logicalParent) {}
    QObject *parentViewModel() const override { return m_logicalParent.data(); }
    int changes = 0;
protected:
    void setParentViewModel(QObject *parent) override
    {
        if (m_logicalParent != parent) {
            m_logicalParent = parent;
            ++changes;
        }
    }
private:
    QPointer<QObject> m_logicalParent;
};

// 只用于协议测试：可以设置逻辑关系、记录委托结果，不接管测试对象。
class CloseRequestParent : public ConductorBase
{
public:
    int calls = 0;
    bool throws = false;
    bool requestedClose = false;
    ViewModelBase *target = nullptr;
    void attach(ViewModelBase *child, QObject *parent) { setLogicalParent(child, parent); }
    QList<ViewModelBase *> getChildren() const override { return {}; }
    void activateItem(ViewModelBase *) override {}
    void deactivateItem(ViewModelBase *item, bool close) override
    {
        ++calls; target = item; requestedClose = close;
        if (throws) throw std::runtime_error("关闭请求失败");
    }
};

template<class C, class U, class = void> struct CanSelect : std::false_type {};
template<class C, class U>
struct CanSelect<C, U, std::void_t<decltype(std::declval<C &>().activateItem(std::declval<U *>()))>> : std::true_type {};
template<class C, class U, class = void> struct CanDeactivate : std::false_type {};
template<class C, class U>
struct CanDeactivate<C, U, std::void_t<decltype(std::declval<C &>().deactivateItem(std::declval<U *>(), false))>> : std::true_type {};
template<class C, class = void> struct CanSetParent : std::false_type {};
template<class C>
struct CanSetParent<C, std::void_t<decltype(std::declval<C &>().setParentViewModel(nullptr))>> : std::true_type {};
static_assert(CanSelect<Conductor<ScreenViewModel>, ProtocolScreen>::value);
static_assert(!CanSelect<Conductor<ScreenViewModel>, ViewModelBase>::value);
static_assert(!CanSelect<Conductor<ScreenViewModel>, const ProtocolScreen>::value);
static_assert(!CanDeactivate<Conductor<ScreenViewModel>, ViewModelBase>::value);
static_assert(!CanSetParent<ScreenViewModel>::value);
static_assert(!CanSetParent<IChild>::value);
static_assert(std::is_abstract_v<ConductorBase>);

class ParentProtocolTests : public QObject
{
    Q_OBJECT
private slots:
    void interfacesAndReadonlyParent()
    {
        ScreenViewModel screen;
        ViewModelBase plain;
        Conductor<> single;
        Conductor<>::Collection::OneActive collection;
        QCOMPARE(qobject_cast<IChild *>(&screen), static_cast<IChild *>(&screen));
        QVERIFY(!qobject_cast<IChild *>(&plain));
        QCOMPARE(qobject_cast<IConductor *>(&single), static_cast<IConductor *>(&single));
        QCOMPARE(qobject_cast<IParent *>(&single), static_cast<IParent *>(&single));
        QVERIFY(qobject_cast<IConductor *>(&collection));
        QVERIFY(qobject_cast<IParent *>(&collection));
        QVERIFY(qobject_cast<IChild *>(&single));
        const auto property = screen.metaObject()->property(screen.metaObject()->indexOfProperty("parentViewModel"));
        QVERIFY(property.isReadable() && !property.isWritable() && property.hasNotifySignal());
        QVERIFY(!screen.parentViewModel());
        QCOMPARE(ConductorViewModelBase::staticMetaObject.superClass(), &ConductorBase::staticMetaObject);
        QCOMPARE(ConductorCollectionOneActiveViewModelBase::staticMetaObject.superClass(), &ConductorBase::staticMetaObject);
    }

    void tryCloseDelegatesOnlyToLogicalParent()
    {
        CloseRequestParent manager;
        QObject unrelated;
        ProtocolScreen screen;
        screen.activate();
        QSignalSpy windowRequests(&screen, &ScreenViewModel::closeRequested);
        screen.setParent(&manager);
        screen.tryClose(); // QObject 所有权不能代替逻辑 Parent。
        QCOMPARE(windowRequests.count(), 1);
        QCOMPARE(manager.calls, 0);
        QVERIFY(screen.isActive());
        manager.attach(&screen, &unrelated);
        screen.tryClose();
        QCOMPARE(manager.calls, 0);
        manager.attach(&screen, &manager);
        screen.tryClose();
        QCOMPARE(manager.calls, 1);
        QCOMPARE(manager.target, &screen);
        QVERIFY(manager.requestedClose && screen.isActive());
        QCOMPARE(screen.parentViewModel(), &manager);
        screen.tryClose();
        QCOMPARE(manager.calls, 2);
        manager.attach(&screen, &unrelated);
        screen.tryClose(); // 每次重新读取 Parent。
        QCOMPARE(windowRequests.count(), 1); // 非 Conductor 的逻辑 Parent 不发窗口请求。
        QCOMPARE(manager.calls, 2);
        manager.attach(&screen, &manager);
        manager.throws = true;
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, screen.tryClose());
        QCOMPARE(manager.calls, 3);
        QCOMPARE(screen.closed, 0);
        QVERIFY(screen.isActive());
        QCOMPARE(screen.metaObject()->indexOfMethod("tryClose()"), -1);
    }

    void tryCloseSingleCurrentAndRetained_data()
    {
        QTest::addColumn<bool>("retained");
        QTest::newRow("current") << false;
        QTest::newRow("retained") << true;
    }
    void tryCloseSingleCurrentAndRetained()
    {
        QFETCH(bool, retained);
        int destroyed = 0;
        Conductor<ProtocolScreen> c;
        c.activate();
        auto owned = std::make_unique<ProtocolScreen>();
        owned->destroyed = &destroyed;
        QPointer<ProtocolScreen> page = owned.get();
        c.activateItem(std::move(owned));
        ProtocolScreen *other = nullptr;
        if (retained) {
            c.deactivateItem(page.data(), false);
            c.activateItem(std::make_unique<ProtocolScreen>());
            other = c.activeItem();
        }
        QSignalSpy selected(&c, &ConductorViewModelBase::activeItemChanged);
        QSignalSpy result(&c, &ConductorBase::activationProcessed);
        QSignalSpy parent(page.data(), &ScreenViewModel::parentViewModelChanged);
        page->tryClose();
        QCOMPARE(page->parent(), &c);
        QCOMPARE(c.activeItem(), other);
        QCOMPARE(selected.count(), retained ? 0 : 1);
        QCOMPARE(parent.count(), retained ? 0 : 1);
        QCOMPARE(result.count(), 0);
        QCOMPARE(page->closed, retained ? 0 : 1);
        QCOMPARE(page->parentViewModel(), retained ? &c : nullptr);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCOMPARE(bool(page), retained);
        QCOMPARE(destroyed, retained ? 0 : 1);
    }

    void tryCloseCollectionAndNestedConductor()
    {
        int destroyed = 0;
        auto outer = std::make_unique<Conductor<ScreenViewModel>::Collection::OneActive>();
        auto inner = std::make_unique<Conductor<ProtocolScreen>>();
        auto *middle = inner.get();
        auto leaf = std::make_unique<ProtocolScreen>();
        leaf->destroyed = &destroyed;
        QPointer<ProtocolScreen> page = leaf.get();
        middle->activateItem(std::move(leaf));
        outer->activateItem(std::move(inner));
        outer->activateItem(std::make_unique<ProtocolScreen>());
        auto *neighbor = outer->activeItem();
        outer->activateItem(middle);
        outer->activate();
        QSignalSpy processed(outer.get(), &ConductorBase::activationProcessed);
        QPointer<ScreenViewModel> pending = middle;
        middle->tryClose();
        QCOMPARE(outer->activeItem(), neighbor);
        QCOMPARE(outer->getChildren(), QList<ViewModelBase *>{neighbor});
        QVERIFY(neighbor->isActive());
        QVERIFY(pending && !pending->parentViewModel());
        QVERIFY(page && !page->parentViewModel() && page->closeSawEmptyParent);
        QCOMPARE(page->closed, 1);
        QCOMPARE(processed.count(), 1);
        QCOMPARE(processed.first().at(0).value<ViewModelBase *>(), neighbor);
        middle->tryClose();
        outer.reset(); // 删除事件之前父树兜底，不能双重回收。
        QVERIFY(!pending && !page);
        QCOMPARE(destroyed, 1);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCOMPARE(destroyed, 1);
    }

    void suspendAndRestoreSingle()
    {
        Conductor<ScreenViewModel> c;
        IConductor *api = &c;
        c.activate();
        auto owned = std::make_unique<ProtocolScreen>();
        auto *page = owned.get();
        c.activateItem(std::move(owned));
        QSignalSpy parent(page, &ScreenViewModel::parentViewModelChanged);
        QSignalSpy active(&c, &ConductorViewModelBase::activeItemChanged);
        QSignalSpy processed(&c, &ConductorBase::activationProcessed);
        api->deactivateItem(page, false);
        QVERIFY(!c.activeItem() && api->getChildren().isEmpty());
        QCOMPARE(page->parentViewModel(), &c);
        QCOMPARE(page->parent(), &c);
        QVERIFY(!page->isActive() && c.isActive());
        QCOMPARE(page->initialized, 1);
        QCOMPARE(page->closed, 0);
        QCOMPARE(active.count(), 1);
        QCOMPARE(parent.count(), 0);
        QCOMPARE(processed.count(), 0);
        api->activateItem(nullptr);
        api->deactivateItem(page, false);
        QCOMPARE(active.count(), 1);
        api->activateItem(page);
        QCOMPARE(c.activeItem(), page);
        QCOMPARE(api->getChildren(), QList<ViewModelBase *>{page});
        QVERIFY(page->isActive());
        QCOMPARE(page->initialized, 1);
        QCOMPARE(parent.count(), 0);
        QCOMPARE(processed.count(), 1);
        c.deactivate(); // 父普通停用保留选择。
        QCOMPARE(c.activeItem(), page);
        c.activate();
        QVERIFY(page->isActive());
    }

    void businessCanReactivateOldObjectAndClosesCurrent()
    {
        Conductor<ProtocolScreen> c;
        c.activate();
        auto first = std::make_unique<ProtocolScreen>();
        auto *a = first.get();
        c.activateItem(std::move(first));
        c.deactivateItem(a, false);
        auto second = std::make_unique<ProtocolScreen>();
        auto *b = second.get();
        c.activateItem(std::move(second));
        QPointer<ProtocolScreen> old = b;
        c.activateItem(a);
        QCOMPARE(a->initialized, 1);
        QCOMPARE(b->closed, 1);
        QVERIFY(b->closeSawEmptyParent);
        QCOMPARE(b->parent(), &c);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(!old);
        QCOMPARE(c.activeItem(), a);
    }

    void closeNonCurrentOldObjectIsNoOp()
    {
        Conductor<ProtocolScreen> c;
        c.activate();
        c.activateItem(std::make_unique<ProtocolScreen>());
        auto *a = c.activeItem();
        c.deactivateItem(a, false);
        c.activateItem(std::make_unique<ProtocolScreen>());
        auto *b = c.activeItem();
        QSignalSpy selected(&c, &ConductorViewModelBase::activeItemChanged);
        QSignalSpy processed(&c, &ConductorBase::activationProcessed);
        c.closeItem(a);
        QCOMPARE(c.activeItem(), b);
        QCOMPARE(a->closed, 0);
        QCOMPARE(a->parentViewModel(), &c);
        QCOMPARE(selected.count(), 0);
        QCOMPARE(processed.count(), 0);
        c.closeItem(a);
    }

    void parentDoesNotCloseOldObjects_data()
    {
        QTest::addColumn<bool>("initialized");
        QTest::newRow("initialized") << true;
        QTest::newRow("uninitialized") << false;
    }
    void parentDoesNotCloseOldObjects()
    {
        QFETCH(bool, initialized);
        int destroyed = 0;
        auto c = std::make_unique<Conductor<ProtocolScreen>>();
        if (initialized) c->activate();
        QList<QPointer<ProtocolScreen>> pages;
        for (int i = 0; i < 3; ++i) {
            auto next = std::make_unique<ProtocolScreen>();
            next->destroyed = &destroyed;
            auto *page = next.get(); c->activateItem(std::move(next));
            pages << page; c->deactivateItem(page, false);
        }
        QVERIFY(c->getChildren().isEmpty());
        c->deactivate(true);
        for (const auto &page : pages) {
            QVERIFY(page); QCOMPARE(page->closed, 0);
            QCOMPARE(page->parentViewModel(), c.get());
        }
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCOMPARE(destroyed, 0);
        c.reset(); QCOMPARE(destroyed, 3);
        for (const auto &page : pages) QVERIFY(!page);
    }

    void parentClosesOnlyCurrent()
    {
        int destroyed = 0;
        QStringList events;
        auto c = std::make_unique<Conductor<ProtocolScreen>>(); c->activate();
        QList<QPointer<ProtocolScreen>> pages;
        for (int i = 0; i < 3; ++i) {
            auto next = std::make_unique<ProtocolScreen>();
            next->name = QString::number(i); next->events = &events; next->destroyed = &destroyed;
            auto *page = next.get(); c->activateItem(std::move(next)); pages << page;
            if (i < 2) c->deactivateItem(page, false);
        }
        events.clear();
        connect(c.get(), &ConductorViewModelBase::activeItemChanged, c.get(), [&] {
            QVERIFY(!c->activeItem() && c->getChildren().isEmpty());
            QCOMPARE(pages[0]->parentViewModel(), c.get());
            QCOMPARE(pages[1]->parentViewModel(), c.get());
            QVERIFY(!pages[2]->parentViewModel());
            events << "selection";
        });
        QSignalSpy result(c.get(), &ConductorBase::activationProcessed);
        c->deactivate(true);
        QCOMPARE(events, QStringList({"selection", "2:close"}));
        QCOMPARE(result.count(), 0);
        QCOMPARE(pages[0]->closed, 0); QCOMPARE(pages[1]->closed, 0); QCOMPARE(pages[2]->closed, 1);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCOMPARE(destroyed, 1); QVERIFY(pages[0] && pages[1] && !pages[2]);
        c.reset(); QCOMPARE(destroyed, 3);
    }

    void collectionParentNotificationOrder()
    {
        QStringList events;
        Conductor<ProtocolScreen>::Collection::OneActive c;
        c.activate();
        c.activateItem(std::make_unique<ProtocolScreen>());
        auto *a = c.activeItem();
        auto next = std::make_unique<ProtocolScreen>();
        auto *b = next.get();
        b->name = "b"; b->events = &events;
        connect(b, &ScreenViewModel::parentViewModelChanged, &c, [&] {
            if (b->parentViewModel()) {
                QCOMPARE(c.getChildren(), (QList<ViewModelBase *>{a, b}));
                QCOMPARE(c.activeItem(), b);
                events << "parent";
            } else {
                QCOMPARE(c.getChildren(), QList<ViewModelBase *>{a});
                QCOMPARE(c.activeItem(), a);
                events << "detach";
            }
        });
        connect(&c, &ConductorCollectionOneActiveViewModelBase::itemsChanged, &c, [&] {
            QCOMPARE(a->parentViewModel(), &c);
            QCOMPARE(b->parentViewModel(), c.activeItem() == b ? &c : nullptr);
            events << "items";
        });
        connect(&c, &ConductorCollectionOneActiveViewModelBase::activeItemChanged, &c, [&] { events << "selection"; });
        connect(&c, &ConductorBase::activationProcessed, &c, [&](ViewModelBase *item, bool success) {
            QVERIFY(success && item == c.activeItem()); events << "processed";
        });
        c.activateItem(std::move(next));
        QCOMPARE(events, QStringList({"parent", "items", "selection", "b:activate", "processed"}));
        events.clear();
        c.deactivateItem(b, true);
        QCOMPARE(events, QStringList({"detach", "items", "selection", "b:close", "processed"}));
        QVERIFY(b->closeSawEmptyParent);
    }

    void parentTreeAndUnexpectedDestruction()
    {
        int destroyed = 0;
        auto c = std::make_unique<Conductor<ProtocolScreen>>();
        c->activate();
        auto add = [&] {
            auto page = std::make_unique<ProtocolScreen>();
            page->destroyed = &destroyed;
            auto *raw = page.get();
            c->activateItem(std::move(page));
            return c->activeItem() == raw ? raw : nullptr;
        };
        auto *a = add();
        QVERIFY(a); c->deactivateItem(a, false);
        auto *b = add();
        QSignalSpy changed(c.get(), &ConductorViewModelBase::activeItemChanged);
        delete a;
        QCOMPARE(changed.count(), 0);
        QCOMPARE(c->activeItem(), b);
        delete b;
        QCOMPARE(changed.count(), 1);
        QVERIFY(!c->activeItem());
        QPointer<ProtocolScreen> pending = add();
        c->closeItem(pending.data());
        QPointer<ProtocolScreen> retained = add();
        c->deactivateItem(retained.data(), false);
        QPointer<ProtocolScreen> current = add();
        c.reset();
        QVERIFY(!pending && !retained && !current);
        QCOMPARE(destroyed, 5);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCOMPARE(destroyed, 5);
    }

    void logicalParentAndTransitionOrder()
    {
        Conductor<ProtocolScreen> c;
        c.activate();
        QStringList events;
        auto first = std::make_unique<ProtocolScreen>();
        first->name = "a"; first->events = &events;
        auto *a = first.get();
        connect(a, &ScreenViewModel::parentViewModelChanged, &c, [&] {
            events << (a->parentViewModel() ? "a:parent" : "a:detach");
        });
        c.activateItem(std::move(first));
        events.clear();
        auto second = std::make_unique<ProtocolScreen>();
        second->name = "b"; second->events = &events;
        auto *b = second.get();
        connect(b, &ScreenViewModel::parentViewModelChanged, &c, [&] { events << "b:parent"; });
        connect(&c, &ConductorViewModelBase::activeItemChanged, &c, [&] {
            QVERIFY(!a->parentViewModel());
            QCOMPARE(b->parentViewModel(), &c);
            QCOMPARE(c.getChildren(), QList<ViewModelBase *>{b});
            events << "selection";
        });
        connect(&c, &ConductorBase::activationProcessed, &c, [&](ViewModelBase *item, bool success) {
            QCOMPARE(item, b); QVERIFY(success && b->isActive()); events << "processed";
        });
        c.activateItem(std::move(second));
        QCOMPARE(events, QStringList({"a:detach", "b:parent", "selection", "a:close", "b:activate", "processed"}));
    }

    void customChildAndLogicalParentRejection()
    {
        Conductor<> c;
        auto owned = std::make_unique<CustomChild>();
        auto *child = owned.get();
        c.activateItem(std::move(owned));
        QCOMPARE(child->parentViewModel(), &c);
        QCOMPARE(child->changes, 1);
        c.deactivateItem(child, false);
        c.activateItem(child);
        QCOMPARE(child->changes, 1);
        c.closeItem(child);
        QCOMPARE(child->changes, 2);
        Conductor<>::Collection::OneActive customCollection;
        auto customOwned = std::make_unique<CustomChild>();
        auto *custom = customOwned.get();
        QSignalSpy addedResult(&customCollection, &ConductorBase::activationProcessed);
        QVERIFY(customCollection.addItem(std::move(customOwned)));
        QCOMPARE(custom->parentViewModel(), &customCollection);
        QCOMPARE(addedResult.count(), 0);
        IConductor *customApi = &customCollection;
        customApi->activateItem(custom);
        customApi->deactivateItem(custom, false);
        QCOMPARE(customCollection.activeItem(), custom);
        QCOMPARE(custom->changes, 1);
        customApi->deactivateItem(custom, true);
        QVERIFY(!custom->parentViewModel());
        QCOMPARE(custom->changes, 2);
        QObject owner;
        auto rejected = std::make_unique<CustomChild>(&owner);
        auto *raw = rejected.get();
        QSignalSpy result(&c, &ConductorBase::activationProcessed);
        QTest::ignoreMessage(QtWarningMsg, "Conductor：不能接管已有逻辑 Parent 的对象");
        c.activateItem(std::move(rejected));
        QCOMPARE(rejected.get(), raw);
        QCOMPARE(raw->parentViewModel(), &owner);
        QCOMPARE(result.count(), 1);
        QVERIFY(!result.first().at(1).toBool());
        Conductor<>::Collection::OneActive collection;
        QTest::ignoreMessage(QtWarningMsg, "Collection.OneActive：不能接管已有逻辑 Parent 的对象");
        collection.activateItem(std::move(rejected));
        QCOMPARE(rejected.get(), raw);
    }

    void collectionPublicProtocol()
    {
        Conductor<ProtocolScreen>::Collection::OneActive c;
        IConductor *api = &c;
        c.activate();
        c.activateItem(std::make_unique<ProtocolScreen>());
        auto *a = c.activeItem();
        c.activateItem(std::make_unique<ProtocolScreen>());
        auto *b = c.activeItem();
        QCOMPARE(api->getChildren(), (QList<ViewModelBase *>{a, b}));
        auto snapshot = api->getChildren(); snapshot.clear();
        QCOMPARE(c.items().size(), 2);
        QSignalSpy parent(a, &ScreenViewModel::parentViewModelChanged);
        QSignalSpy selected(&c, &ConductorCollectionOneActiveViewModelBase::activeItemChanged);
        QSignalSpy processed(&c, &ConductorBase::activationProcessed);
        api->deactivateItem(b, false);
        QCOMPARE(c.activeItem(), b);
        QVERIFY(!b->isActive());
        QCOMPARE(b->parentViewModel(), &c);
        QCOMPARE(selected.count(), 0);
        api->activateItem(b);
        QVERIFY(b->isActive());
        QCOMPARE(processed.count(), 1);
        api->activateItem(nullptr);
        QCOMPARE(c.items().size(), 2);
        api->activateItem(b);
        processed.clear();
        api->deactivateItem(b, true);
        QCOMPARE(c.activeItem(), a);
        QVERIFY(a->isActive() && !b->parentViewModel() && b->closeSawEmptyParent);
        QCOMPARE(processed.count(), 1);
        QCOMPARE(processed.first().at(0).value<ViewModelBase *>(), a);
        QCOMPARE(parent.count(), 0);
        api->activateItem(b);
        api->deactivateItem(b, true);
        c.deactivate(true);
        QVERIFY(!a->parentViewModel() && a->closeSawEmptyParent);
        QCOMPARE(parent.count(), 1);
    }

    void resultSignals_data()
    {
        QTest::addColumn<bool>("collection");
        QTest::newRow("single") << false;
        QTest::newRow("collection") << true;
    }
    void resultSignals()
    {
        QFETCH(bool, collection);
        std::unique_ptr<ConductorBase> owner;
        ProtocolScreen *page = nullptr;
        if (collection) owner = std::make_unique<Conductor<ProtocolScreen>::Collection::OneActive>();
        else owner = std::make_unique<Conductor<ProtocolScreen>>();
        QSignalSpy result(owner.get(), &ConductorBase::activationProcessed);
        auto next = std::make_unique<ProtocolScreen>(); page = next.get();
        if (collection) static_cast<Conductor<ProtocolScreen>::Collection::OneActive *>(owner.get())->activateItem(std::move(next));
        else static_cast<Conductor<ProtocolScreen> *>(owner.get())->activateItem(std::move(next));
        QCOMPARE(result.count(), 1); // 非活动父对象也公布新选择。
        owner->activateItem(page);
        QCOMPARE(result.count(), 1); // 非活动父对象重复同项不公布。
        owner->activate();
        QCOMPARE(result.count(), 1);
        owner->activateItem(page);
        QCOMPARE(result.count(), 2);
        ViewModelBase foreign;
        owner->activateItem(&foreign);
        QCOMPARE(result.count(), 3);
        QCOMPARE(result.last().at(0).value<ViewModelBase *>(), &foreign);
        QVERIFY(!result.last().at(1).toBool());
        owner->deactivateItem(&foreign, false);
        owner->deactivateItem(nullptr, true);
        QCOMPARE(result.count(), 3);
        owner->activateItem(nullptr);
        QCOMPARE(result.count(), 3);
    }

    void plainVmAndNestedConductors()
    {
        Conductor<> single;
        single.activateItem(std::make_unique<ViewModelBase>());
        auto *plain = single.activeItem();
        single.deactivateItem(plain, false);
        QVERIFY(single.getChildren().isEmpty());
        single.activateItem(plain);
        Conductor<>::Collection::OneActive outer;
        auto inner = std::make_unique<Conductor<ProtocolScreen>>();
        auto *middle = inner.get();
        middle->activateItem(std::make_unique<ProtocolScreen>());
        auto *leaf = middle->activeItem();
        outer.activateItem(std::move(inner));
        QCOMPARE(middle->parentViewModel(), &outer);
        QCOMPARE(leaf->parentViewModel(), middle);
        outer.activate();
        middle->deactivateItem(leaf, false);
        QVERIFY(middle->getChildren().isEmpty());
        QCOMPARE(leaf->parentViewModel(), middle);
        outer.deactivate(true);
        QVERIFY(!middle->parentViewModel());
        QCOMPARE(leaf->parentViewModel(), middle);
        QCOMPARE(leaf->closed, 0);
    }
};

QTEST_GUILESS_MAIN(ParentProtocolTests)
#include "tst_parent_protocol.moc"
