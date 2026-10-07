#include <CaliburnMicroQt/Conductor.h>
#include <QQmlEngine>
#include <QThread>
#include <QtTest>

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
    void onDeactivate(bool close) override
    {
        record(close ? "close" : "deactivate");
    }
private:
    void record(const char *event) { if (events) *events << name + ':' + event; }
};

class PlainVm : public ViewModelBase
{
public:
    explicit PlainVm(int *destroyed = nullptr) : destroyed(destroyed) {}
    ~PlainVm() override { if (destroyed) ++*destroyed; }
    int *destroyed;
};

template<class C, class U, class = void>
struct CanAdopt : std::false_type {};
template<class C, class U>
struct CanAdopt<C, U, std::void_t<decltype(std::declval<C &>().activateItem(
    std::declval<std::unique_ptr<U> &&>()))>> : std::true_type {};

static_assert(std::is_same_v<decltype(std::declval<Conductor<TrackedScreen> &>().activeItem()), TrackedScreen *>);
static_assert(CanAdopt<Conductor<>, PlainVm>::value);
static_assert(CanAdopt<Conductor<ScreenViewModel>, TrackedScreen>::value);
static_assert(!CanAdopt<Conductor<ScreenViewModel>, PlainVm>::value);
static_assert(!CanAdopt<Conductor<>, QObject>::value);
static_assert(!CanAdopt<Conductor<>, const PlainVm>::value);
static_assert(!std::is_copy_constructible_v<Conductor<>>);

class ConductorTests : public QObject
{
    Q_OBJECT
private slots:
    void initialStateAndPlainItems()
    {
        int destroyed = 0;
        Conductor<> conductor;
        QVERIFY(!conductor.activeItem());
        QVERIFY(!conductor.isInitialized());
        QVERIFY(!conductor.isActive());
        QSignalSpy changed(&conductor, &ConductorViewModelBase::activeItemChanged);
        QVERIFY(conductor.activateItem(nullptr));
        QCOMPARE(changed.count(), 0);
        auto item = std::make_unique<PlainVm>(&destroyed);
        auto *raw = item.get();
        QQmlEngine::setObjectOwnership(raw, QQmlEngine::JavaScriptOwnership);
        QVERIFY(conductor.activateItem(std::move(item)));
        QVERIFY(!item);
        QCOMPARE(conductor.activeItem(), raw);
        QCOMPARE(raw->parent(), &conductor);
        QCOMPARE(QQmlEngine::objectOwnership(raw), QQmlEngine::CppOwnership);
        conductor.activate();
        conductor.deactivate();
        conductor.activate();
        QPointer<PlainVm> weak = raw;
        QVERIFY(conductor.closeItem(raw));
        QVERIFY(!conductor.activeItem());
        QVERIFY(weak);
        QCOMPARE(weak->parent(), &conductor);
        QCOMPARE(changed.count(), 2);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(!weak);
        QCOMPARE(destroyed, 1);
    }

    void switchOrderAndDeferredDeletion()
    {
        QStringList events;
        int destroyed = 0;
        Conductor<ScreenViewModel> conductor;
        conductor.activate();
        auto a = std::make_unique<TrackedScreen>("a", &events, &destroyed);
        auto *old = a.get();
        QVERIFY(conductor.activateItem(std::move(a)));
        auto b = std::make_unique<TrackedScreen>("b", &events);
        auto *next = b.get();
        bool observedIntermediate = false;
        connect(&conductor, &ConductorViewModelBase::activeItemChanged, this, [&] {
            events << "changed";
            observedIntermediate = conductor.activeItem() == next && old->isActive()
                && !next->isInitialized() && next->parent() == &conductor
                && QQmlEngine::objectOwnership(next) == QQmlEngine::CppOwnership;
        });
        events.clear();
        QPointer<TrackedScreen> weak = old;
        QVERIFY(conductor.activateItem(std::move(b)));
        QVERIFY(observedIntermediate);
        QCOMPARE(events, QStringList({"changed", "a:close", "b:initialize", "b:activate"}));
        QVERIFY(!old->isActive());
        QVERIFY(next->isActive());
        QVERIFY(weak);
        QCOMPARE(destroyed, 0);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(!weak);
        QCOMPARE(destroyed, 1);
        QCOMPARE(conductor.activeItem(), next);
    }

    void inactiveSelectionAndParentLifecycle()
    {
        QStringList events;
        Conductor<TrackedScreen> conductor;
        auto item = std::make_unique<TrackedScreen>("child", &events);
        auto *child = item.get();
        QVERIFY(conductor.activateItem(std::move(item)));
        QSignalSpy changed(&conductor, &ConductorViewModelBase::activeItemChanged);
        conductor.initialize();
        QVERIFY(!child->isInitialized());
        QVERIFY(events.isEmpty());
        conductor.activate();
        conductor.activate();
        conductor.deactivate();
        conductor.deactivate();
        QCOMPARE(conductor.activeItem(), child);
        QVERIFY(!child->isActive());
        conductor.activate();
        QVERIFY(child->isActive());
        QCOMPARE(changed.count(), 0);
        QPointer<TrackedScreen> weak = child;
        conductor.deactivate(true);
        conductor.deactivate(true);
        QVERIFY(!conductor.activeItem());
        QVERIFY(weak);
        QCOMPARE(child->parent(), &conductor);
        QVERIFY(!child->isActive());
        QCOMPARE(changed.count(), 1);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(!weak);
        conductor.activate();
        QVERIFY(conductor.isActive());
        QVERIFY(!conductor.activeItem());
        QCOMPARE(events, QStringList({"child:initialize", "child:activate", "child:deactivate",
                                     "child:activate", "child:close"}));
    }


    void parentCloseFollowsInitialization_data()
    {
        QTest::addColumn<int>("state");
        QTest::addColumn<bool>("screenItem");
        for (int state = 0; state < 5; ++state) {
            for (bool screen : {false, true}) {
                const QByteArray name = QByteArray::number(state) + (screen ? "-screen" : "-plain");
                QTest::newRow(name.constData()) << state << screen;
            }
        }
    }

    void parentCloseFollowsInitialization()
    {
        QFETCH(int, state);
        QFETCH(bool, screenItem);
        int destroyed = 0;
        QStringList events;
        Conductor<> conductor;
        if (state == 1)
            conductor.initialize();
        else if (state >= 2)
            conductor.activate();
        if (state == 4)
            conductor.deactivate(true); // 已关闭父对象后再接入新项，也必须能清理。
        if (screenItem)
            QVERIFY(conductor.activateItem(std::make_unique<TrackedScreen>("child", &events, &destroyed)));
        else
            QVERIFY(conductor.activateItem(std::make_unique<PlainVm>(&destroyed)));
        if (state == 3)
            conductor.deactivate();
        const bool initialized = conductor.isInitialized();
        auto *old = conductor.activeItem();
        QPointer<ViewModelBase> weak = old;
        auto *screen = qobject_cast<ScreenViewModel *>(old);
        const bool childActive = screen && screen->isActive();
        events.clear();
        QSignalSpy changed(&conductor, &ConductorViewModelBase::activeItemChanged);
        connect(&conductor, &ConductorViewModelBase::activeItemChanged, this, [&] {
            QVERIFY(!conductor.activeItem());
            QVERIFY(weak);
            if (screen)
                QCOMPARE(screen->isActive(), childActive);
            events << "changed";
        });
        conductor.deactivate(true);
        if (!initialized) {
            // CM 跳过未初始化父对象的关闭，保留当前项和 QObject 所有权。
            conductor.deactivate(true);
            QCOMPARE(conductor.activeItem(), old);
            QVERIFY(!conductor.isInitialized());
            QVERIFY(!conductor.isActive());
            QVERIFY(events.isEmpty());
            QCOMPARE(changed.count(), 0);
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            QVERIFY(weak);
            QCOMPARE(destroyed, 0);
            QCOMPARE(old->parent(), &conductor);
            if (screen)
                QVERIFY(!screen->isInitialized());
            // 后续显式初始化父对象后可以正常关闭，仍不提前初始化子项。
            conductor.initialize();
            conductor.deactivate(true);
        }
        QVERIFY(!conductor.activeItem());
        QVERIFY(!conductor.isActive());
        QVERIFY(conductor.isInitialized());
        QVERIFY(weak);
        QCOMPARE(old->parent(), &conductor);
        QCOMPARE(destroyed, 0);
        const bool childInitialized = screenItem && (state == 2 || state == 3);
        QCOMPARE(events, childInitialized ? QStringList({"changed", "child:close"}) : QStringList({"changed"}));
        conductor.deactivate(true);
        QCOMPARE(changed.count(), 1);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(!weak);
        QCOMPARE(destroyed, 1);
        conductor.activate();
        QVERIFY(conductor.isActive());
        QVERIFY(!conductor.activeItem());
    }

    void parentCloseBeforeDeferredDeletion()
    {
        int destroyed = 0;
        auto conductor = std::make_unique<Conductor<>>();
        QVERIFY(conductor->activateItem(std::make_unique<PlainVm>(&destroyed)));
        QPointer<ViewModelBase> weak = conductor->activeItem();
        conductor->initialize();
        conductor->deactivate(true);
        QVERIFY(!conductor->activeItem());
        QVERIFY(weak);
        conductor.reset();
        QVERIFY(!weak);
        QCOMPARE(destroyed, 1);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCOMPARE(destroyed, 1);
    }

    void uninitializedParentDestruction()
    {
        int destroyed = 0;
        QStringList events;
        auto conductor = std::make_unique<Conductor<TrackedScreen>>();
        QVERIFY(conductor->activateItem(std::make_unique<TrackedScreen>("child", &events, &destroyed)));
        QPointer<TrackedScreen> weak = conductor->activeItem();
        conductor->deactivate(true);
        QCOMPARE(conductor->activeItem(), weak.data());
        conductor.reset();
        QVERIFY(!weak);
        QCOMPARE(destroyed, 1);
        QVERIFY(events.isEmpty()); // 父树回收不补初始化或关闭生命周期。
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCOMPARE(destroyed, 1);
    }

    void replaceWhileInactive()
    {
        QStringList events;
        Conductor<TrackedScreen> conductor;
        conductor.activate();
        QVERIFY(conductor.activateItem(std::make_unique<TrackedScreen>("a", &events)));
        conductor.deactivate();
        events.clear();
        QVERIFY(conductor.activateItem(std::make_unique<TrackedScreen>("b", &events)));
        QCOMPARE(events, QStringList({"a:close"}));
        QVERIFY(!conductor.activeItem()->isInitialized());
        conductor.activate();
        QCOMPARE(events, QStringList({"a:close", "b:initialize", "b:activate"}));
    }

    void clearAndClose_data()
    {
        QTest::addColumn<int>("mode");
        QTest::newRow("null-literal") << 0;
        QTest::newRow("empty-owner") << 1;
        QTest::newRow("close-current") << 2;
    }

    void clearAndClose()
    {
        QFETCH(int, mode);
        QStringList events;
        Conductor<TrackedScreen> conductor;
        conductor.activate();
        QVERIFY(conductor.activateItem(std::make_unique<TrackedScreen>("a", &events)));
        auto *old = conductor.activeItem();
        QPointer<TrackedScreen> weak = old;
        connect(&conductor, &ConductorViewModelBase::activeItemChanged, this, [&] {
            QVERIFY(!conductor.activeItem());
            QVERIFY(old->isActive());
            events << "changed";
        });
        events.clear();
        if (mode == 0)
            QVERIFY(conductor.activateItem(nullptr));
        else if (mode == 1) {
            std::unique_ptr<TrackedScreen> empty;
            QVERIFY(conductor.activateItem(std::move(empty)));
        } else
            QVERIFY(conductor.closeItem(old));
        QCOMPARE(events, QStringList({"changed", "a:close"}));
        QVERIFY(weak);
        QVERIFY(!conductor.closeItem(old));
        QVERIFY(!conductor.closeItem(nullptr));
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(!weak);
    }

    void rejectedCandidatesKeepOwnership()
    {
        Conductor<> conductor;
        QVERIFY(conductor.activateItem(std::make_unique<PlainVm>()));
        auto *current = conductor.activeItem();
        QSignalSpy changed(&conductor, &ConductorViewModelBase::activeItemChanged);
        QObject parent;
        auto parented = std::make_unique<PlainVm>();
        parented->setParent(&parent);
        QTest::ignoreMessage(QtWarningMsg, "Conductor：接管对象必须无父对象且与 Conductor 位于同一线程");
        QVERIFY(!conductor.activateItem(std::move(parented)));
        QVERIFY(parented);
        QCOMPARE(parented->parent(), &parent);
        auto active = std::make_unique<TrackedScreen>();
        active->activate();
        QTest::ignoreMessage(QtWarningMsg, "Conductor：不能接管已经激活的 Screen");
        QVERIFY(!conductor.activateItem(std::move(active)));
        QVERIFY(active);
        QVERIFY(active->isActive());
        QVERIFY(!conductor.closeItem(active.get()));
        QCOMPARE(conductor.activeItem(), current);
        QCOMPARE(changed.count(), 0);
    }

    void rejectsAncestorOwnership()
    {
        auto self = std::make_unique<Conductor<>>();
        auto *selfRaw = self.get();
        QTest::ignoreMessage(QtWarningMsg, "Conductor：不能接管自身或祖先对象");
        QVERIFY(!selfRaw->activateItem(std::move(self)));
        QCOMPARE(self.get(), selfRaw);
        auto ancestor = std::make_unique<Conductor<>>();
        auto *child = new Conductor<>(ancestor.get());
        QTest::ignoreMessage(QtWarningMsg, "Conductor：不能接管自身或祖先对象");
        QVERIFY(!child->activateItem(std::move(ancestor)));
        QVERIFY(ancestor);
        QCOMPARE(child->parent(), ancestor.get());
    }

    void rejectsDifferentThreadOwnership()
    {
        Conductor<> conductor;
        // 对象在工作线程创建、移动与销毁，不从主线程删除异线程对象。
        PlainVm *foreign = nullptr;
        bool foreignRejected = false;
        QThread foreignThread;
        QObject context;
        context.moveToThread(&foreignThread);
        foreignThread.start();
        QMetaObject::invokeMethod(&context, [&] { foreign = new PlainVm; }, Qt::BlockingQueuedConnection);
        std::unique_ptr<PlainVm> foreignOwner(foreign);
        QTest::ignoreMessage(QtWarningMsg, "Conductor：接管对象必须无父对象且与 Conductor 位于同一线程");
        foreignRejected = !conductor.activateItem(std::move(foreignOwner));
        const bool retained = foreignOwner.get() == foreign;
        foreignOwner.release();
        QMetaObject::invokeMethod(&context, [&] {
            delete foreign;
            context.moveToThread(QCoreApplication::instance()->thread());
        }, Qt::BlockingQueuedConnection);
        foreignThread.quit();
        QVERIFY(foreignThread.wait(5000));
        QVERIFY(foreignRejected && retained);
    }

    void unexpectedDestruction()
    {
        Conductor<> conductor;
        QVERIFY(conductor.activateItem(std::make_unique<PlainVm>()));
        QSignalSpy changed(&conductor, &ConductorViewModelBase::activeItemChanged);
        delete conductor.activeItem();
        QVERIFY(!conductor.activeItem());
        QCOMPARE(changed.count(), 1);
        QVERIFY(conductor.activateItem(std::make_unique<PlainVm>()));
        QVERIFY(conductor.activateItem(std::make_unique<PlainVm>()));
        auto *current = conductor.activeItem();
        const int before = changed.count();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCOMPARE(conductor.activeItem(), current);
        QCOMPARE(changed.count(), before);
    }

    void parentReclaimsPendingAndCurrentOnce()
    {
        int destroyed = 0;
        QStringList events;
        QPointer<TrackedScreen> first, second;
        {
            Conductor<TrackedScreen> conductor;
            conductor.activate();
            QVERIFY(conductor.activateItem(std::make_unique<TrackedScreen>("a", &events, &destroyed)));
            first = conductor.activeItem();
            QVERIFY(conductor.activateItem(std::make_unique<TrackedScreen>("b", &events, &destroyed)));
            second = conductor.activeItem();
            QCOMPARE(destroyed, 0);
            events.clear();
        }
        QVERIFY(!first && !second);
        QCOMPARE(destroyed, 2);
        QVERIFY(events.isEmpty()); // 析构不补关闭钩子。
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCOMPARE(destroyed, 2);
    }

    void nestedConductor()
    {
        QStringList events;
        Conductor<ScreenViewModel> outer;
        auto inner = std::make_unique<Conductor<TrackedScreen>>();
        auto *nested = inner.get();
        QVERIFY(inner->activateItem(std::make_unique<TrackedScreen>("leaf", &events)));
        auto *leaf = inner->activeItem();
        QVERIFY(outer.activateItem(std::move(inner)));
        outer.activate();
        QVERIFY(nested->isActive() && leaf->isActive());
        outer.deactivate();
        QVERIFY(!nested->isActive() && !leaf->isActive());
        QPointer<ScreenViewModel> weak = nested;
        QPointer<TrackedScreen> weakLeaf = leaf;
        QSignalSpy outerChanged(&outer, &ConductorViewModelBase::activeItemChanged);
        QSignalSpy innerChanged(nested, &ConductorViewModelBase::activeItemChanged);
        outer.deactivate(true);
        QVERIFY(!outer.activeItem());
        QVERIFY(!nested->activeItem());
        QVERIFY(weak && weakLeaf);
        QCOMPARE(outerChanged.count(), 1);
        QCOMPARE(innerChanged.count(), 1);
        QCOMPARE(events, QStringList({"leaf:initialize", "leaf:activate", "leaf:deactivate", "leaf:close"}));
        outer.deactivate(true);
        QCOMPARE(outerChanged.count(), 1);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(!weak && !weakLeaf);
        outer.activate();
        QVERIFY(outer.isActive());
        QVERIFY(!outer.activeItem());
    }

    void metaObjectContract()
    {
        Conductor<> conductor;
        const auto *meta = conductor.metaObject();
        const int index = meta->indexOfProperty("activeItem");
        QVERIFY(index >= 0);
        const auto property = meta->property(index);
        QVERIFY(property.isReadable());
        QVERIFY(!property.isWritable());
        QVERIFY(property.hasNotifySignal());
        QCOMPARE(property.metaType(), QMetaType::fromType<ViewModelBase *>());
        QVERIFY(meta->indexOfMethod("activateItem()") < 0);
        QVERIFY(meta->indexOfMethod("closeItem()") < 0);
    }
};

QTEST_GUILESS_MAIN(ConductorTests)
#include "tst_conductor.moc"
