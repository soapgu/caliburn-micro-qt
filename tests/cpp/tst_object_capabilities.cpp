#include <CaliburnMicroQt/BootstrapperBase.h>
#include <CaliburnMicroQt/Conductor.h>
#include <CaliburnMicroQt/ViewLocator.h>
#include <CaliburnMicroQt/WindowManager.h>
#include "../support/DialogWindowSupport.h"
#include <QQmlExtensionPlugin>
#include <QQuickStyle>
#include <QTimer>
#include <QtTest>
#include <stdexcept>

Q_IMPORT_QML_PLUGIN(CaliburnMicroQtPlugin)

struct ObjectStats {
    int activations = 0;
    int deactivations = 0;
    int closes = 0;
    int destroyed = 0;
    QStringList events;
};

class ObjectModel : public QObject
{
    Q_OBJECT
public:
    ObjectStats *stats = nullptr;
    ~ObjectModel() override { if (stats) { ++stats->destroyed; stats->events << "destroy"; } }
};

class ActivationObject : public ObjectModel, public IActivate
{
    Q_OBJECT
    Q_INTERFACES(IActivate)
public:
    bool isActive() const override { return active; }
    void activate() override { active = true; if (stats) ++stats->activations; }
    bool active = false;
};

class DeactivationObject : public ObjectModel, public IDeactivate
{
    Q_OBJECT
    Q_INTERFACES(IDeactivate)
public:
    bool failClose = false;
    void deactivate(bool close) override {
        emit attempting(close);
        if (stats) {
            if (close) { ++stats->closes; stats->events << "close"; }
            else ++stats->deactivations;
        }
        if (close && failClose) throw std::runtime_error("普通对象关闭失败");
        emit completed(close);
    }
    QMetaObject::Connection subscribeAttemptingDeactivation(
            QObject *context, DeactivationCallback callback) override
    { return connect(this, &DeactivationObject::attempting, context, std::move(callback)); }
    QMetaObject::Connection subscribeDeactivated(
            QObject *context, DeactivationCallback callback) override
    { return connect(this, &DeactivationObject::completed, context, std::move(callback)); }
signals:
    void attempting(bool close);
    void completed(bool close);
};

class GuardObject : public ObjectModel, public IGuardClose
{
    Q_OBJECT
    Q_INTERFACES(IGuardClose)
public:
    int checks = 0;
    bool failGuard = false;
    CloseCallback pending;
    void canClose(CloseCallback callback) override {
        ++checks;
        if (failGuard) throw std::runtime_error("普通对象守卫失败");
        pending = std::move(callback);
    }
    void decide(bool allowed) { auto callback = std::move(pending); callback(allowed); }
};

class ChildObject : public ObjectModel, public IChild
{
    Q_OBJECT
    Q_INTERFACES(IChild)
public:
    QObject *parentViewModel() const override { return logicalParent.data(); }
protected:
    void setParentViewModel(QObject *parent) override { logicalParent = parent; }
private:
    QPointer<QObject> logicalParent;
};

class PlainWindow : public ObjectModel { Q_OBJECT };
class ActivationWindow : public ActivationObject { Q_OBJECT };
class DeactivationWindow : public DeactivationObject { Q_OBJECT };
class GuardWindow : public GuardObject { Q_OBJECT };
class ChildWindow : public ChildObject { Q_OBJECT };

static std::unique_ptr<ObjectModel> makeObject(int kind, bool window)
{
    switch (kind) {
    case 0: if (window) return std::make_unique<PlainWindow>(); return std::make_unique<ObjectModel>();
    case 1: if (window) return std::make_unique<ActivationWindow>(); return std::make_unique<ActivationObject>();
    case 2: if (window) return std::make_unique<DeactivationWindow>(); return std::make_unique<DeactivationObject>();
    case 3: if (window) return std::make_unique<GuardWindow>(); return std::make_unique<GuardObject>();
    default: if (window) return std::make_unique<ChildWindow>(); return std::make_unique<ChildObject>();
    }
}

class ObjectBootstrapper : public BootstrapperBase
{
public:
    ObjectBootstrapper(QGuiApplication &app, int kind, ObjectStats &stats)
        : BootstrapperBase(app), kind(kind), stats(stats) {}
    QPointer<ObjectModel> model;
    QPointer<QQuickWindow> window;
    bool typedInjection = false;
    bool aliveAtExit = false;
protected:
    bool configure() override {
        switch (kind) {
        case 0: return factory<PlainWindow>();
        case 1: return factory<ActivationWindow>();
        case 2: return factory<DeactivationWindow>();
        case 3: return factory<GuardWindow>();
        default: return factory<ChildWindow>();
        }
    }
    void onStartup() override {
        bool shown = false;
        switch (kind) {
        case 0: shown = displayRootViewFor<PlainWindow>(); break;
        case 1: shown = displayRootViewFor<ActivationWindow>(); break;
        case 2: shown = displayRootViewFor<DeactivationWindow>(); break;
        case 3: shown = displayRootViewFor<GuardWindow>(); break;
        default: shown = displayRootViewFor<ChildWindow>(); break;
        }
        if (!shown) return;
        window = managedWindow(model);
        typedInjection = window && window->property("viewModel").value<QObject *>() == model;
        QTimer::singleShot(0, this, [this] {
            window->close();
            if (auto *guard = qobject_cast<GuardObject *>(model))
                QTimer::singleShot(0, this, [guard] { guard->decide(true); });
        });
        QTimer::singleShot(2000, this, [] { QCoreApplication::exit(9); });
    }
    void onExit() override {
        aliveAtExit = model && window;
        stats.events << "exit";
    }
private:
    template<class T> bool factory() {
        return registerRootFactory<T>([this](std::shared_ptr<IWindowManager>) {
            auto object = std::make_unique<T>();
            object->stats = &stats;
            model = object.get();
            return object;
        });
    }
    int kind;
    ObjectStats &stats;
};

class ObjectCapabilitiesTests : public QObject
{
    Q_OBJECT
    static void kinds() {
        QTest::addColumn<int>("kind");
        QTest::newRow("普通QObject") << 0;
        QTest::newRow("仅激活") << 1;
        QTest::newRow("仅停用") << 2;
        QTest::newRow("仅守卫") << 3;
        QTest::newRow("仅Parent") << 4;
    }
    template<class C> void conduct(int kind) {
        ObjectStats stats;
        C conductor;
        conductor.activate();
        auto model = makeObject(kind, false);
        model->stats = &stats;
        auto *raw = model.get();
        QPointer<QObject> weak = raw;
        conductor.activateItem(std::move(model));
        QCOMPARE(conductor.activeItem(), raw);
        QCOMPARE(raw->parent(), &conductor);
        QCOMPARE(stats.activations, kind == 1 ? 1 : 0);
        if (auto *child = qobject_cast<IChild *>(raw)) QCOMPARE(child->parentViewModel(), &conductor);
        conductor.closeItem(raw);
        if (auto *guard = qobject_cast<GuardObject *>(raw)) {
            QCOMPARE(guard->checks, 1);
            QCOMPARE(conductor.activeItem(), raw);
            guard->decide(false);
            QCOMPARE(conductor.activeItem(), raw);
            conductor.closeItem(raw);
            guard->decide(true);
        }
        QVERIFY(!conductor.activeItem());
        QCOMPARE(stats.closes, kind == 2 ? 1 : 0);
        QVERIFY(weak); // 关闭不自动删除已接管对象。
        delete raw;
        QVERIFY(!weak);
        QCOMPARE(stats.destroyed, 1);
    }
private slots:
    void initTestCase() {
        QQuickStyle::setStyle("Basic");
        qmlRegisterUncreatableType<ObjectModel>("ObjectTests", 1, 0, "ObjectModel", "测试模型由 C++ 创建");
        const QUrl dialog("qrc:/tests/fixtures/ObjectDialog.qml");
        const QUrl window("qrc:/tests/fixtures/ObjectWindow.qml");
        QVERIFY(ViewLocator::registerView<ObjectModel>(dialog));
        QVERIFY(ViewLocator::registerView<ActivationObject>(dialog));
        QVERIFY(ViewLocator::registerView<DeactivationObject>(dialog));
        QVERIFY(ViewLocator::registerView<GuardObject>(dialog));
        QVERIFY(ViewLocator::registerView<ChildObject>(dialog));
        QVERIFY(ViewLocator::registerView<PlainWindow>(window));
        QVERIFY(ViewLocator::registerView<ActivationWindow>(window));
        QVERIFY(ViewLocator::registerView<DeactivationWindow>(window));
        QVERIFY(ViewLocator::registerView<GuardWindow>(window));
        QVERIFY(ViewLocator::registerView<ChildWindow>(window));
        QVERIFY(ViewLocator::freeze());
        QGuiApplication::setQuitOnLastWindowClosed(false);
    }
    void single_data() { kinds(); }
    void single() { QFETCH(int, kind); conduct<Conductor<>>(kind); }
    void collection_data() { kinds(); }
    void collection() { QFETCH(int, kind); conduct<Conductor<>::Collection::OneActive>(kind); }

    void ordinaryWindow_data() { kinds(); }
    void ordinaryWindow() {
        QFETCH(int, kind);
        ObjectStats stats;
        auto model = makeObject(kind, true);
        model->stats = &stats;
        WindowManager manager;
        WindowCleanup cleanup{manager};
        QVERIFY(manager.showWindow(QVariant::fromValue(model.get())));
        QPointer<QQuickWindow> window = managedWindow(model.get());
        QVERIFY(window && window->isVisible());
        QCOMPARE(window->property("viewModel").value<QObject *>(), model.get());
        QCOMPARE(stats.activations, kind == 1 ? 1 : 0);
        QVERIFY(!manager.busy() && !manager.currentDialog());
        window->close();
        if (auto *guard = qobject_cast<GuardObject *>(model.get())) {
            QTRY_COMPARE(guard->checks, 1);
            guard->decide(false);
            QVERIFY(window->isVisible());
            window->close();
            QTRY_COMPARE(guard->checks, 2);
            guard->decide(true);
        }
        QTRY_VERIFY(!window->isVisible());
        QCOMPARE(stats.closes, kind == 2 ? 1 : 0);
        manager.releaseWindows();
        QVERIFY(!window);
        QCOMPARE(stats.destroyed, 0);
    }

    void dialog_data() { kinds(); }
    void dialog() {
        QFETCH(int, kind);
        ObjectStats stats;
        PlainWindow root;
        WindowManager manager;
        WindowCleanup cleanup{manager};
        auto *owner = showManagedWindow(manager, &root);
        QVERIFY(owner);
        auto model = makeObject(kind, false);
        auto *raw = model.get();
        QPointer<ObjectModel> weak = raw;
        model->stats = &stats;
        auto future = manager.showDialogAsync(std::move(model), &root);
        QCOMPARE(manager.currentDialog(), raw);
        QVERIFY(manager.busy());
        auto *window = dialogWindow(owner);
        QVERIFY(window);
        QCOMPARE(stats.activations, kind == 1 ? 1 : 0);
        window->close();
        if (auto *guard = qobject_cast<GuardObject *>(raw)) {
            QTRY_COMPARE(guard->checks, 1);
            guard->decide(false);
            QVERIFY(!future.isFinished() && window->isVisible());
            window->close();
            QTRY_COMPARE(guard->checks, 2);
            guard->decide(true);
        }
        QTRY_VERIFY(future.isFinished());
        QCOMPARE(future.result(), DialogResult{});
        QCOMPARE(stats.closes, kind == 2 ? 1 : 0);
        QVERIFY(!manager.currentDialog() && !manager.busy());
        QTRY_VERIFY(!weak);
        QCOMPARE(stats.destroyed, 1);
    }

    void subscriptionLifetimeAndDisconnect() {
        DeactivationObject object;
        auto receiver = std::make_unique<QObject>();
        int attempts = 0, completions = 0;
        auto *api = qobject_cast<IDeactivate *>(&object);
        api->subscribeAttemptingDeactivation(receiver.get(), [&](bool) { ++attempts; });
        auto connection = api->subscribeDeactivated(receiver.get(), [&](bool) { ++completions; });
        object.deactivate(false);
        QCOMPARE(attempts, 1); QCOMPARE(completions, 1);
        disconnect(connection);
        object.deactivate(true);
        QCOMPARE(attempts, 2); QCOMPARE(completions, 1);
        receiver.reset();
        object.deactivate(true);
        QCOMPARE(attempts, 2);
    }

    void reverseCloseAndException() {
        ObjectStats stats;
        DeactivationWindow root;
        root.stats = &stats;
        WindowManager manager;
        WindowCleanup cleanup{manager};
        auto *window = showManagedWindow(manager, &root);
        QVERIFY(window);
        root.deactivate(false);
        QCoreApplication::processEvents();
        QVERIFY(window->isVisible());
        root.deactivate(true);
        QTRY_VERIFY(!window->isVisible());
        QCOMPARE(stats.closes, 1);
        manager.releaseWindows();
        QCOMPARE(stats.closes, 1);

        QVERIFY(showManagedWindow(manager, &root));
        auto model = std::make_unique<DeactivationObject>();
        auto *raw = model.get();
        ObjectStats dialogStats;
        raw->stats = &dialogStats;
        auto future = manager.showDialogAsync(std::move(model), &root);
        raw->deactivate(false);
        QVERIFY(!future.isFinished());
        raw->deactivate(true);
        QTRY_VERIFY(future.isFinished());
        QCOMPARE(future.result(), DialogResult{});
        QCOMPARE(dialogStats.closes, 1);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCOMPARE(dialogStats.destroyed, 1);

        auto failing = std::make_unique<DeactivationObject>();
        failing->stats = &dialogStats;
        failing->failClose = true;
        auto *failed = failing.get();
        auto failedFuture = manager.showDialogAsync(std::move(failing), &root);
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, failed->deactivate(true));
        manager.prepareForShutdown();
        QVERIFY(failedFuture.isFinished());
        QCOMPARE(failedFuture.result(), DialogResult{});
        QCOMPARE(dialogStats.closes, 2); // 清理不重试已经抛异常的关闭。
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

    void guardExceptionAndDestroyedModel() {
        PlainWindow root;
        WindowManager manager;
        WindowCleanup cleanup{manager};
        auto *owner = showManagedWindow(manager, &root);
        QVERIFY(owner);
        auto object = std::make_unique<GuardObject>();
        auto *guard = object.get();
        guard->failGuard = true;
        auto future = manager.showDialogAsync(std::move(object), &root);
        auto *window = dialogWindow(owner);
        QVERIFY(window);
        QTest::ignoreMessage(QtWarningMsg, "WindowConductor: close guard failed: 普通对象守卫失败");
        window->close();
        QTRY_COMPARE(guard->checks, 1);
        QVERIFY(!future.isFinished());
        guard->failGuard = false;
        window->close();
        QTRY_VERIFY(bool(guard->pending));
        auto answer = std::move(guard->pending);
        delete guard;
        QTRY_VERIFY(future.isFinished());
        QCOMPARE(future.result(), DialogResult{});
        answer(true); // 迟到许可不能访问已销毁模型及桥接。
        QCoreApplication::processEvents();
        QVERIFY(!dialogWindow(owner));
    }

    void typedConductorRejectsBaseEntry() {
        Conductor<ActivationObject> single;
        Conductor<ActivationObject>::Collection::OneActive collection;
        for (auto *conductor : {static_cast<ConductorBase *>(&single), static_cast<ConductorBase *>(&collection)}) {
            auto *foreign = new ObjectModel;
            foreign->setParent(conductor);
            QSignalSpy processed(conductor, &ConductorBase::activationProcessed);
            qobject_cast<IConductor *>(conductor)->activateItem(foreign);
            QVERIFY(!static_cast<ConductorBaseWithActiveItem *>(conductor)->activeItem());
            QCOMPARE(processed.count(), 1);
            QCOMPARE(processed.first().at(1).toBool(), false);
            QVERIFY(conductor->setProperty("activeItem", QVariant::fromValue(static_cast<QObject *>(foreign))));
            QVERIFY(conductor->getChildren().isEmpty() || !conductor->getChildren().first());
            delete foreign;
        }
    }

    void rootFactory_data() { kinds(); }
    void rootFactory() {
        QFETCH(int, kind);
        ObjectStats stats;
        QGuiApplication::setQuitOnLastWindowClosed(true);
        ObjectBootstrapper bootstrapper(*qobject_cast<QGuiApplication *>(QCoreApplication::instance()), kind, stats);
        QCOMPARE(bootstrapper.run(), 0);
        QVERIFY(bootstrapper.typedInjection && bootstrapper.aliveAtExit);
        QVERIFY(!bootstrapper.model && !bootstrapper.window);
        QCOMPARE(stats.activations, kind == 1 ? 1 : 0);
        QCOMPARE(stats.closes, kind == 2 ? 1 : 0);
        QCOMPARE(stats.destroyed, 1);
        QVERIFY(stats.events.indexOf("exit") < stats.events.indexOf("destroy"));
        QGuiApplication::setQuitOnLastWindowClosed(false);
    }
};

QTEST_MAIN(ObjectCapabilitiesTests)
#include "tst_object_capabilities.moc"
