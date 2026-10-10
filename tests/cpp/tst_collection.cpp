#include <CaliburnMicroQt/Conductor.h>
#include <QQmlEngine>
#include <QThread>
#include <QtTest>

using Collection = Conductor<ScreenViewModel>::Collection::OneActive;
class TrackedScreen : public ScreenViewModel
{
public:
    explicit TrackedScreen(QString name = {}, QStringList *events = nullptr, int *destroyed = nullptr)
        : name(std::move(name)), events(events), destroyed(destroyed) {}
    ~TrackedScreen() override { if (destroyed) ++*destroyed; }
    QString name;
    QStringList *events;
    int *destroyed;
protected:
    void onInitialize() override { record("initialize"); }
    void onActivate() override { record("activate"); }
    void onDeactivate(bool close) override { record(close ? "close" : "deactivate"); }
private:
    void record(const char *event) { if (events) *events << name + ':' + event; }
};
class PlainVm : public ViewModelBase {};
template<class C, class U, class = void> struct CanAdd : std::false_type {};
template<class C, class U>
struct CanAdd<C, U, std::void_t<decltype(std::declval<C &>().addItem(
    std::declval<std::unique_ptr<U> &&>()))>> : std::true_type {};
static_assert(std::is_same_v<Collection, ConductorCollectionOneActive<ScreenViewModel>>);
static_assert(std::is_same_v<decltype(std::declval<Collection>().items()), QList<ScreenViewModel *>>);
static_assert(std::is_same_v<decltype(std::declval<Collection>().activeItem()), ScreenViewModel *>);
static_assert(CanAdd<Collection, TrackedScreen>::value);
static_assert(!CanAdd<Collection, PlainVm>::value);
static_assert(!CanAdd<Collection, const TrackedScreen>::value);
static_assert(!CanAdd<ConductorCollectionOneActive<>, QObject>::value);
static_assert(!std::is_copy_constructible_v<Collection>);

class CollectionTests : public QObject
{
    Q_OBJECT
private slots:
    void snapshotsAndPlainItems()
    {
        Conductor<>::Collection::OneActive c;
        QVERIFY(c.items().isEmpty() && !c.activeItem());
        QSignalSpy items(&c, &ConductorCollectionOneActiveViewModelBase::itemsChanged);
        QSignalSpy active(&c, &ConductorCollectionOneActiveViewModelBase::activeItemChanged);
        std::unique_ptr<PlainVm> empty;
        QVERIFY(!c.addItem(std::move(empty)));
        c.activateItem(std::move(empty));
        c.activateItem(nullptr);
        QCOMPARE(items.count(), 0);
        QCOMPARE(active.count(), 0);
        auto owned = std::make_unique<PlainVm>();
        auto *first = owned.get();
        QQmlEngine::setObjectOwnership(first, QQmlEngine::JavaScriptOwnership);
        QVERIFY(c.addItem(std::move(owned)));
        QVERIFY(!owned && !c.activeItem());
        QCOMPARE(first->parent(), &c);
        QCOMPARE(QQmlEngine::objectOwnership(first), QQmlEngine::CppOwnership);
        auto snapshot = c.items();
        snapshot.clear();
        QCOMPARE(c.items().size(), 1);
        const auto variants = c.property("items").toList();
        QCOMPARE(variants.size(), 1);
        QCOMPARE(variants.first().value<ViewModelBase *>(), first);
        c.activateItem(first);
        c.activateItem(first);
        c.activateItem(std::make_unique<PlainVm>());
        QCOMPARE(c.items().size(), 2);
        QCOMPARE(items.count(), 2);
        QCOMPARE(active.count(), 2);
        PlainVm foreign;
        c.activateItem(&foreign);
        c.closeItem(&foreign);
        c.closeItem(nullptr);
        c.activateItem(nullptr);
        QCOMPARE(c.items().size(), 2);
        QCOMPARE(active.count(), 3);
        c.closeItem(first);
        QCOMPARE(c.items().size(), 1);
    }

    void switchLifecycleAndNotificationOrder()
    {
        QStringList events;
        Collection c;
        auto a = std::make_unique<TrackedScreen>("a", &events);
        auto *first = a.get();
        QVERIFY(c.addItem(std::move(a)));
        c.activateItem(first);
        c.initialize();
        QVERIFY(!first->isInitialized());
        c.activate();
        QCOMPARE(events, QStringList({"a:initialize", "a:activate"}));
        connect(&c, &Collection::itemsChanged, this, [&] {
            QVERIFY(c.items().contains(c.activeItem()));
            events << "items";
        });
        connect(&c, &Collection::activeItemChanged, this, [&] { events << "active"; });
        events.clear();
        c.activateItem(std::make_unique<TrackedScreen>("b", &events));
        auto *second = c.activeItem();
        QCOMPARE(events, QStringList({"items", "active", "a:deactivate", "b:initialize", "b:activate"}));
        QVERIFY(!first->isActive() && second->isActive());
        QCOMPARE(c.items().size(), 2);
        events.clear();
        c.activateItem(first);
        QCOMPARE(events, QStringList({"active", "b:deactivate", "a:activate"}));
        events.clear();
        c.activateItem(first);
        QVERIFY(events.isEmpty());
        c.deactivate();
        QVERIFY(!first->isActive());
        c.activate();
        QCOMPARE(c.activeItem(), first);
        QCOMPARE(events, QStringList({"a:deactivate", "a:activate"}));
        // 析构时测试事件缓冲仍有效；不在 QObject 析构期发送集合通知。
    }

    void clearSelectionRetainsItems_data()
    {
        QTest::addColumn<bool>("emptyOwner");
        QTest::newRow("nullptr") << false;
        QTest::newRow("empty-owner") << true;
    }
    void clearSelectionRetainsItems()
    {
        QFETCH(bool, emptyOwner);
        QStringList events;
        Collection c;
        c.activateItem(std::make_unique<TrackedScreen>("a", &events));
        auto *first = c.activeItem();
        c.activate();
        events.clear();
        if (emptyOwner) {
            std::unique_ptr<TrackedScreen> empty;
            c.activateItem(std::move(empty));
        } else {
            c.activateItem(nullptr);
        }
        QVERIFY(!c.activeItem());
        QCOMPARE(events, QStringList({"a:deactivate"}));
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCOMPARE(c.items().first(), first);
        c.activateItem(first);
        QCOMPARE(events.last(), QStringLiteral("a:activate"));
    }

    void closeSelection_data()
    {
        QTest::addColumn<int>("count");
        QTest::addColumn<int>("index");
        QTest::addColumn<int>("expected");
        QTest::addColumn<bool>("parentActive");
        for (bool active : {false, true}) {
            QTest::newRow(active ? "first-active" : "first-inactive") << 3 << 0 << 1 << active;
            QTest::newRow(active ? "middle-active" : "middle-inactive") << 3 << 1 << 0 << active;
            QTest::newRow(active ? "last-active" : "last-inactive") << 3 << 2 << 1 << active;
            QTest::newRow(active ? "only-active" : "only-inactive") << 1 << 0 << -1 << active;
        }
    }
    void closeSelection()
    {
        QFETCH(int, count); QFETCH(int, index); QFETCH(int, expected); QFETCH(bool, parentActive);
        QStringList events;
        int destroyed = 0;
        Collection c;
        for (int i = 0; i < count; ++i)
            QVERIFY(c.addItem(std::make_unique<TrackedScreen>(QString::number(i), &events, &destroyed)));
        const auto original = c.items();
        auto *old = original[index];
        c.activateItem(old);
        if (parentActive)
            c.activate();
        else
            c.initialize();
        QPointer<ScreenViewModel> weak = old;
        connect(&c, &Collection::itemsChanged, this, [&] {
            QCOMPARE(c.items().size(), count - 1);
            QCOMPARE(c.activeItem(), expected < 0 ? nullptr : original[expected]);
            events << "items";
        });
        connect(&c, &Collection::activeItemChanged, this, [&] { events << "active"; });
        events.clear();
        c.closeItem(old);
        QStringList wanted{"items", "active"};
        if (parentActive)
            wanted << QString::number(index) + ":close";
        if (parentActive && expected >= 0)
            wanted << QString::number(expected) + ":initialize" << QString::number(expected) + ":activate";
        QCOMPARE(events, wanted);
        QVERIFY(weak);
        c.closeItem(old);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(!weak);
        QCOMPARE(destroyed, 1);
        QCOMPARE(c.activeItem(), expected < 0 ? nullptr : original[expected]);
    }

    void closeInactiveAndParentClose()
    {
        QStringList events;
        int destroyed = 0;
        Collection c;
        c.activateItem(std::make_unique<TrackedScreen>("a", &events, &destroyed));
        auto *first = c.activeItem();
        c.deactivate(true); // 未初始化不清空。
        QCOMPARE(c.items().size(), 1);
        c.activate();
        c.activateItem(std::make_unique<TrackedScreen>("b", &events, &destroyed));
        auto *second = c.activeItem();
        events.clear();
        QSignalSpy selection(&c, &Collection::activeItemChanged);
        c.closeItem(first);
        QCOMPARE(events, QStringList({"a:close"}));
        QCOMPARE(selection.count(), 0);
        QCOMPARE(c.activeItem(), second);
        QVERIFY(c.addItem(std::make_unique<TrackedScreen>("never", &events, &destroyed)));
        QSignalSpy membership(&c, &Collection::itemsChanged);
        events.clear();
        c.deactivate(true);
        c.deactivate(true);
        QVERIFY(c.items().isEmpty() && !c.activeItem());
        QCOMPARE(events, QStringList({"b:close"})); // 从未初始化的项仍沿用 Screen 关闭无操作。
        QCOMPARE(membership.count(), 1);
        QCOMPARE(selection.count(), 1);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCOMPARE(destroyed, 3);
        c.activate();
        QVERIFY(!c.activeItem());
    }

    void parentClosesAllInitializedScreensOnce()
    {
        QStringList events;
        Collection c;
        c.activate();
        c.activateItem(std::make_unique<TrackedScreen>("a", &events));
        auto *a = c.activeItem();
        c.activateItem(std::make_unique<TrackedScreen>("b", &events));
        auto *b = c.activeItem();
        QSignalSpy selected(&c, &Collection::activeItemChanged);
        QSignalSpy members(&c, &Collection::itemsChanged);
        events.clear();
        c.deactivate(true);
        c.deactivate(true);
        QCOMPARE(events, QStringList({"a:close", "b:close"}));
        QCOMPARE(members.count(), 1);
        QCOMPARE(selected.count(), 1);
        QVERIFY(!a->isActive() && !b->isActive());
        QVERIFY(c.items().isEmpty() && !c.activeItem());
    }

    void unexpectedDeletionDoesNotNavigate()
    {
        Collection c;
        c.activateItem(std::make_unique<TrackedScreen>());
        auto *first = c.activeItem();
        c.activateItem(std::make_unique<TrackedScreen>());
        auto *second = c.activeItem();
        c.activate();
        QSignalSpy members(&c, &Collection::itemsChanged);
        QSignalSpy selected(&c, &Collection::activeItemChanged);
        delete first;
        QCOMPARE(c.activeItem(), second);
        QCOMPARE(members.count(), 1);
        QCOMPARE(selected.count(), 0);
        QVERIFY(c.addItem(std::make_unique<TrackedScreen>()));
        auto *remaining = c.items().last();
        delete second;
        QVERIFY(!c.activeItem());
        QCOMPARE(c.items().size(), 1);
        QVERIFY(!remaining->isActive());
        QCOMPARE(selected.count(), 1);
        c.activateItem(remaining);
    }

    void ownershipRejection()
    {
        ConductorCollectionOneActive<> c;
        QObject parent;
        auto owned = std::make_unique<PlainVm>();
        owned->setParent(&parent);
        QTest::ignoreMessage(QtWarningMsg, "Collection.OneActive：接管对象必须无父对象且位于同一线程");
        QVERIFY(!c.addItem(std::move(owned)));
        QVERIFY(owned && owned->parent() == &parent);
        auto active = std::make_unique<TrackedScreen>();
        active->activate();
        QTest::ignoreMessage(QtWarningMsg, "Collection.OneActive：不能接管已经激活的 Screen");
        c.activateItem(std::move(active));
        QVERIFY(active && active->isActive());
        auto self = std::make_unique<ConductorCollectionOneActive<>>();
        QTest::ignoreMessage(QtWarningMsg, "Collection.OneActive：不能接管自身或祖先对象");
        QVERIFY(!self->addItem(std::move(self)));
        QVERIFY(self);
        auto ancestor = std::make_unique<ConductorCollectionOneActive<>>();
        auto *child = new ConductorCollectionOneActive<>(ancestor.get());
        QTest::ignoreMessage(QtWarningMsg, "Collection.OneActive：不能接管自身或祖先对象");
        child->activateItem(std::move(ancestor));
        QVERIFY(ancestor);
        QVERIFY(c.items().isEmpty());
    }

    void rejectsOtherThread()
    {
        Collection c;
        QThread worker;
        QObject context;
        context.moveToThread(&worker);
        auto candidate = std::make_unique<TrackedScreen>();
        candidate->moveToThread(&worker);
        worker.start();
        QTest::ignoreMessage(QtWarningMsg, "Collection.OneActive：接管对象必须无父对象且位于同一线程");
        auto *original = candidate.get();
        const bool adopted = c.addItem(std::move(candidate));
        const bool retained = candidate.get() == original;
        // 无论断言结果如何，先在对象所属线程安排回收并终止线程。
        QPointer<TrackedScreen> weak = candidate.get();
        if (candidate) {
            auto *raw = candidate.release();
            QMetaObject::invokeMethod(&context, [raw, &context] {
                delete raw;
                context.moveToThread(QCoreApplication::instance()->thread());
            }, Qt::BlockingQueuedConnection);
        }
        worker.quit();
        QVERIFY(worker.wait(3000));
        QVERIFY(!adopted && retained && !weak);
        QVERIFY(c.items().isEmpty());
    }

    void nestedAndParentTreeReclaimsOnce()
    {
        int destroyed = 0;
        auto outer = std::make_unique<Collection>();
        auto inner = std::make_unique<Collection>();
        auto *nested = inner.get();
        inner->activateItem(std::make_unique<TrackedScreen>("leaf", nullptr, &destroyed));
        QPointer<ScreenViewModel> leaf = inner->activeItem();
        outer->activateItem(std::move(inner));
        outer->activate();
        QVERIFY(nested->isActive() && leaf->isActive());
        outer->deactivate();
        QVERIFY(!nested->isActive() && !leaf->isActive());
        outer->activate();
        outer->closeItem(nested);
        QVERIFY(nested->items().isEmpty());
        outer->activateItem(std::make_unique<TrackedScreen>("new", nullptr, &destroyed));
        QPointer<ScreenViewModel> current = outer->activeItem();
        QSignalSpy changed(outer.get(), &Collection::activeItemChanged);
        outer.reset(); // 父树回收等待删除的项和当前项。
        QVERIFY(!leaf && !current);
        QCOMPARE(destroyed, 2);
        QCOMPARE(changed.count(), 0);
    }
};
QTEST_GUILESS_MAIN(CollectionTests)
#include "tst_collection.moc"
