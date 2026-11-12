#include <CaliburnMicroQt/BootstrapperBase.h>
#include <CaliburnMicroQt/WindowManager.h>
#include <CaliburnMicroQt/ViewLocator.h>
#include <CaliburnMicroQt/ScreenViewModel.h>
#include <CaliburnMicroQt/ConfirmActionViewModel.h>
#include "../support/TestInstanceProvider.h"
#include <QGuiApplication>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QQuickWindow>
#include <QQuickStyle>
#include <QThread>
#include <QTimer>
#include <QtTest>
#include <tuple>

Q_IMPORT_QML_PLUGIN(CaliburnMicroQtPlugin)

struct Observations {
    QStringList order;
    QString scenario;
    bool skipMappings = false;
    ~Observations() { if (worker.isRunning()) { worker.quit(); worker.wait(); } }
    QObject parent;
    QThread worker;
    int resolves = 0, creates = 0, factories = 0, exits = 0;
    int rootsDestroyed = 0, prepares = 0, releases = 0, subscriptions = 0, notifications = 0;
    int rootsCreated = 0, activations = 0, closeAttempts = 0, closeHooks = 0;
    bool noAssociationAtRootDestruction = false, noViewAtRootDestruction = false, viewObserved = false;
    QObject *rootAddress = nullptr; // 仅作 windowFor 的身份查询，不解引用已销毁对象。
    auto counts() const {
        return std::make_tuple(resolves, creates, factories, exits, rootsCreated, rootsDestroyed,
                               activations, closeAttempts, closeHooks, prepares, releases,
                               subscriptions, notifications, order.size());
    }
    bool factorySameService = false;
    bool startup = false, loop = false, typed = false, sameService = false, aliveAtExit = false;
    QPointer<QObject> root;
    QPointer<QQuickWindow> view;
    std::weak_ptr<IInstanceProvider> provider;
    std::weak_ptr<IWindowManager> windows;
    std::shared_ptr<IWindowManager> retained;
};

class ProviderRoot : public ScreenViewModel
{
    Q_OBJECT
public:
    explicit ProviderRoot(Observations &state) : state(state) {
        ++state.rootsCreated;
        state.root = this;
        state.rootAddress = this;
        connect(this, &ScreenViewModel::attemptingDeactivation, this, [&state](bool close) {
            if (close) { ++state.closeAttempts; state.order << "root.closeAttempt"; }
        });
    }
    ~ProviderRoot() override {
        const auto service = state.windows.lock();
        state.noAssociationAtRootDestruction = !service || !service->windowFor(this);
        state.noViewAtRootDestruction = !state.view;
        ++state.rootsDestroyed;
        state.order << "root.destroy";
    }
protected:
    void onActivate() override {
        ++state.activations;
        state.order << "root.activate";
        const auto service = state.windows.lock();
        state.view = service ? service->windowFor(this) : nullptr;
        state.viewObserved = bool(state.view);
        if (state.view) connect(state.view, &QObject::destroyed, this, [this] { state.order << "view.destroy"; });
        if (state.scenario == "activateThrows") throw std::runtime_error("根激活异常");
    }
    void onDeactivate(bool close) override {
        if (close) { ++state.closeHooks; state.order << "root.closeHook"; }
        if (close && state.scenario == "rootCloseThrows") throw std::runtime_error("根关闭异常");
    }
public:
    Observations &state;
};

class ProviderWindows : public WindowManager
{
public:
    explicit ProviderWindows(Observations &state) : state(state) {}
    ~ProviderWindows() override { state.order << "service.destroy"; }
    void prepareForShutdown() override {
        ++state.prepares;
        state.order << "prepare";
        WindowManager::prepareForShutdown();
        if (state.scenario == "prepareThrows" && !withinRelease) throw std::runtime_error("准备退出异常");
    }
    void releaseWindows() override {
        ++state.releases;
        state.order << "release";
        withinRelease = true;
        WindowManager::releaseWindows();
        withinRelease = false;
        state.order << "release.complete";
        if (state.scenario == "cleanupSignal") emit windowCleanupFailed();
        if (state.scenario == "releaseThrows" || state.scenario == "exitCodeAndCleanup" || state.scenario == "exitAndReleaseThrows")
            throw std::runtime_error("释放窗口异常");
    }
    QMetaObject::Connection subscribeWindowCleanupFailed(QObject *context, std::function<void()> callback) override {
        ++state.subscriptions;
        if (state.scenario == "subscriptionFailure") return {};
        if (state.scenario == "subscriptionThrows") throw std::runtime_error("订阅异常");
        return WindowManager::subscribeWindowCleanupFailed(context, [this, callback = std::move(callback)] {
            ++state.notifications;
            callback();
        });
    }
    Observations &state;
    bool withinRelease = false;
};
class LegacyProviderWindows : public IWindowManager
{
public:
    bool showWindow(const QVariant &, const WindowOptions &, const std::optional<QString> &) override { return false; }
    QQuickWindow *windowFor(QObject *) const override { return nullptr; }
    bool busy() const override { return false; }
    QObject *currentDialog() const override { return nullptr; }
    QFuture<DialogResult> showDialogAsync(std::unique_ptr<QObject>, QObject *, const DialogOptions &,
                                         const std::optional<QString> &) override { return {}; }
};
class BootstrapProvider : public TestInstanceProvider
{
public:
    explicit BootstrapProvider(Observations &state) : state(state) {}
    ~BootstrapProvider() override { state.order << "provider.destroy"; }
protected:
    std::optional<SharedInstance> resolveShared(const InstanceRequest &request) override {
        ++state.resolves;
        state.order << "resolve";
        if (state.scenario == "serviceThrows") throw std::runtime_error("窗口服务解析异常");
        return TestInstanceProvider::resolveShared(request);
    }
private:
    Observations &state;
};

class ProviderBootstrapper : public BootstrapperBase
{
public:
    ProviderBootstrapper(QGuiApplication &app, Observations &state) : BootstrapperBase(app), state(state) {}
    bool install(std::shared_ptr<IInstanceProvider> provider) { return setInstanceProvider(std::move(provider)); }
protected:
    bool configure() override {
        state.order << "configure.begin";
        auto provider = std::make_shared<BootstrapProvider>(state);
        state.provider = provider;
        if (state.scenario == "nullSet") return install({});
        if (!install(provider)) return false;
        if (state.scenario == "repeatSet") return install(provider);
        if (state.scenario == "configureFailure") return false;
        if (state.scenario == "configureThrows") throw std::runtime_error("配置异常");
        std::shared_ptr<IWindowManager> windows;
        if (state.scenario == "missingLifecycle") windows = std::make_shared<LegacyProviderWindows>();
        else windows = std::make_shared<ProviderWindows>(state);
        state.windows = windows;
        if (state.scenario == "serviceParent") windows->setParent(&state.parent);
        if (state.scenario == "serviceThread") {
            state.worker.start();
            windows->moveToThread(&state.worker);
            state.retained = windows;
        }
        if (state.scenario.startsWith("retained")) state.retained = windows;
        if (state.scenario != "serviceMissing") {
            provider->add({typeid(IWindowManager), {}}, [this, windows] {
                if (state.scenario == "serviceWrongContract")
                    return SharedInstance::from<QObject>(std::make_shared<QObject>());
                return SharedInstance::from<IWindowManager>(windows);
            });
        }
        provider->uniqueFactory = [this](const InstanceRequest &request) {
            ++state.creates;
            state.order << "create";
            if (request.type != typeid(ProviderRoot)) throw std::logic_error("错误的根请求");
            if (state.scenario == "rootMissing")
                throw InstanceError(InstanceErrorCode::NotRegistered, InstanceOperation::Create, request,
                                    QStringLiteral("根未登记"));
            if (state.scenario == "rootThrows" || state.scenario == "recoverAfterRootFailure") throw std::runtime_error("根创建异常");
            if (state.scenario == "rootNull") return UniqueInstance::from<ProviderRoot>(std::unique_ptr<ProviderRoot>{});
            if (state.scenario == "rootWrongContract") return UniqueInstance::from<QObject>(std::make_unique<QObject>());
            auto root = std::make_unique<ProviderRoot>(state);
            if (state.scenario == "rootParent") root->setParent(&state.parent);
            if (state.scenario == "rootThread") {
                state.worker.start();
                root->moveToThread(&state.worker);
            }
            return UniqueInstance::from<ProviderRoot>(std::move(root));
        };
        if (state.scenario.startsWith("legacy")) {
            if (!registerRootFactory<ProviderRoot>([this](std::shared_ptr<IWindowManager> service) {
                ++state.factories;
                state.factorySameService = service == state.windows.lock();
                if (state.scenario == "legacyNull") return std::unique_ptr<ProviderRoot>{};
                if (state.scenario == "legacyThrows") throw std::runtime_error("旧工厂异常");
                return std::make_unique<ProviderRoot>(state);
            })) return false;
        }
        if (!state.skipMappings) {
            const auto view = (state.scenario == "showFailure" || state.scenario == "retainedShowFailure") ? "qrc:/tests/missing-provider.qml"
                                                             : "qrc:/tests/fixtures/ProviderRoot.qml";
            if (!ViewLocator::registerView<ProviderRoot>(QUrl(view))) return false;
            if (state.scenario == "recoverAfterDisplayFailure" &&
                !ViewLocator::registerView<ProviderRoot>(QUrl("qrc:/tests/missing-provider.qml"), QStringLiteral("Broken")))
                return false;
        }
        // 此时只有登记，不允许已经向提供器解析。
        if (state.resolves || state.creates) return false;
        state.order << "configure.end";
        return true;
    }
    void onStartup() override {
        state.startup = true;
        state.order << "startup";
        const auto context = state.scenario == "recoverAfterDisplayFailure"
                ? std::optional<QString>(QStringLiteral("Broken")) : std::nullopt;
        if (!displayRootViewFor<ProviderRoot>({}, context)) return;
        auto windows = state.windows.lock();
        auto *root = qobject_cast<ProviderRoot *>(state.root.data());
        state.view = windows->windowFor(root);
        state.sameService = state.sameService || (state.view && state.view->property("_caliburnWindowManager").value<QObject *>() == windows.get());
        state.typed = state.view && state.view->property("viewModel").metaType() == QMetaType::fromType<ProviderRoot *>()
                && state.view->property("viewModel").value<ProviderRoot *>() == root;
        if (state.scenario == "startupAfterDisplayThrows" || state.scenario == "retainedStartupFailure")
            throw std::runtime_error("根显示后启动异常");
        QTimer::singleShot(0, this, [this, windows, root] {
            state.loop = true;
            if (state.scenario == "dialogIdentity") {
                auto future = windows->showDialogAsync(std::make_unique<ConfirmActionViewModel>(
                        ConfirmationRequest{"测试", "共享服务"}), root);
                QVERIFY(windows->busy() && windows->currentDialog());
                qobject_cast<ScreenViewModel *>(windows->currentDialog())->tryClose(true);
                QTRY_VERIFY(future.isFinished());
            }
            if (state.scenario == "normalClose") state.view->close();
            else QCoreApplication::exit(state.scenario == "exitCodeAndCleanup" ? 7 : 0);
        });
    }
    void onExit() override {
        ++state.exits;
        state.order << "exit";
        state.aliveAtExit = state.root && state.view;
        if (state.scenario == "exitThrows" || state.scenario == "exitAndReleaseThrows" ||
            state.scenario == "retainedExitFailure" || state.scenario == "recoverAfterExitFailure")
            throw std::runtime_error("退出钩子异常");
    }
private:
    Observations &state;
};

class BootstrapperProviderTests : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase() {
        QQuickStyle::setStyle("Basic");
        qmlRegisterUncreatableType<ProviderRoot>("ProviderTests", 1, 0, "ProviderRoot", "由 C++ 创建");
    }
    void startup_data() {
        QTest::addColumn<QString>("scenario");
        for (const char *name : {"success", "normalClose", "dialogIdentity", "legacy", "legacyNull", "legacyThrows",
                "nullSet", "repeatSet", "configureFailure", "configureThrows", "serviceMissing", "serviceThrows", "missingLifecycle",
                "serviceParent", "serviceThread", "subscriptionFailure", "subscriptionThrows", "rootMissing", "rootThrows",
                "rootNull", "rootWrongContract", "rootParent", "rootThread", "showFailure",
                "prepareThrows", "releaseThrows", "cleanupSignal", "exitCodeAndCleanup", "retained", "sequential",
                "serviceWrongContract", "activateThrows", "startupAfterDisplayThrows", "rootCloseThrows",
                "exitThrows", "exitAndReleaseThrows", "retainedShowFailure", "retainedStartupFailure",
                "retainedExitFailure", "recoverAfterRootFailure", "recoverAfterDisplayFailure", "recoverAfterExitFailure"})
            QTest::newRow(name) << QString(name);
    }
    void startup() {
        QFETCH(QString, scenario);
        const QStringList successes{"success", "normalClose", "dialogIdentity", "legacy", "retained", "sequential"};
        auto *app = qobject_cast<QGuiApplication *>(qApp);
        const bool quitPolicy = app->quitOnLastWindowClosed();
        auto run = [&](Observations &state) {
            auto bootstrapper = std::make_unique<ProviderBootstrapper>(*app, state);
            QVERIFY(!bootstrapper->install(std::make_shared<TestInstanceProvider>()));
            const int expected = state.scenario == "exitCodeAndCleanup" ? 7 : (successes.contains(state.scenario) ? 0 : 1);
            QTimer watchdog;
            watchdog.setSingleShot(true);
            QObject::connect(&watchdog, &QTimer::timeout, this, [] { QCoreApplication::exit(99); });
            watchdog.start(10000);
            QCOMPARE(bootstrapper->run(), expected);
            watchdog.stop();
            QVERIFY(!bootstrapper->install(std::make_shared<TestInstanceProvider>()));
            QCOMPARE(bootstrapper->run(), 1);
            QCOMPARE(state.exits, 1);
            QVERIFY(state.provider.expired());
            QVERIFY(!state.root && !state.view);
            QVERIFY(QGuiApplication::allWindows().isEmpty());
            QCOMPARE(app->quitOnLastWindowClosed(), quitPolicy);
            const bool rejectedSetting = state.scenario == "nullSet" || state.scenario == "repeatSet" || state.scenario == "configureFailure" || state.scenario == "configureThrows";
            QCOMPARE(state.resolves, rejectedSetting ? 0 : 1);
            if (state.resolves) {
                QVERIFY(state.order.indexOf("configure.end") < state.order.indexOf("resolve"));
            }
            const QStringList rejectedService{"serviceMissing", "serviceThrows", "missingLifecycle", "serviceParent",
                    "serviceThread", "subscriptionFailure", "subscriptionThrows", "serviceWrongContract"};
            QCOMPARE(state.startup, !rejectedSetting && !rejectedService.contains(state.scenario));
            if (state.startup) QVERIFY(state.order.indexOf("resolve") < state.order.indexOf("startup"));
            const QStringList beforeLoopFailures{"rootMissing", "rootThrows", "rootNull", "rootWrongContract",
                    "rootParent", "rootThread", "showFailure", "legacyNull", "legacyThrows", "activateThrows",
                    "startupAfterDisplayThrows", "retainedShowFailure", "retainedStartupFailure",
                    "recoverAfterRootFailure", "recoverAfterDisplayFailure"};
            QCOMPARE(state.loop, state.startup && !beforeLoopFailures.contains(state.scenario));
            if (state.scenario.startsWith("legacy")) {
                QCOMPARE(state.factories, 1);
                QCOMPARE(state.creates, 0);
                QVERIFY(state.factorySameService);
            } else {
                QCOMPARE(state.factories, 0);
                QCOMPARE(state.creates, state.startup ? 1 : 0);
            }
            if (state.loop) {
                QVERIFY(state.typed && state.sameService && state.aliveAtExit);
                QCOMPARE(state.rootsDestroyed, 1);
                QVERIFY(state.order.indexOf("exit") < state.order.indexOf("view.destroy"));
                QVERIFY(state.order.indexOf("view.destroy") < state.order.indexOf("root.destroy"));
                QVERIFY(state.order.indexOf("root.destroy") < state.order.indexOf("provider.destroy"));
            }
            if (state.scenario == "rootParent" || state.scenario == "rootThread" || state.scenario == "showFailure")
                QCOMPARE(state.rootsDestroyed, 1);
            if (state.scenario == "serviceThread") {
                QCOMPARE(state.prepares, 0);
                QCOMPARE(state.releases, 0);
                auto retained = state.retained;
                QMetaObject::invokeMethod(retained.get(), [retained, app] { retained->moveToThread(app->thread()); },
                                          Qt::BlockingQueuedConnection);
            } else if (state.scenario == "serviceParent" || state.scenario.startsWith("subscription")) {
                QCOMPARE(state.prepares, 2); // 显式准备一次，releaseWindows 内部再次准备。
                QCOMPARE(state.releases, 1);
            }
            QVERIFY(state.rootsCreated <= 1);
            QCOMPARE(state.rootsDestroyed, state.rootsCreated);
            QCOMPARE(state.viewObserved, state.activations > 0);
            if (state.releases) {
                QCOMPARE(state.order.count("release.complete"), 1);
                QVERIFY(state.order.indexOf("exit") < state.order.indexOf("release.complete"));
            }
            QCOMPARE(state.closeAttempts, state.activations ? 1 : 0);
            QCOMPARE(state.closeHooks, state.closeAttempts);
            if (state.rootsCreated) {
                QVERIFY(state.noAssociationAtRootDestruction);
                QVERIFY(state.noViewAtRootDestruction);
                QVERIFY(state.order.indexOf("root.destroy") < state.order.indexOf("provider.destroy"));
                if (state.scenario != "rootParent" && state.scenario != "rootThread")
                    QVERIFY(state.order.indexOf("release.complete") < state.order.indexOf("root.destroy"));
            }
            if (state.viewObserved) {
                QCOMPARE(state.order.count("view.destroy"), 1);
                QVERIFY(state.order.indexOf("view.destroy") < state.order.indexOf("root.destroy"));
                if (state.scenario == "activateThrows")
                    QVERIFY(state.order.indexOf("view.destroy") < state.order.indexOf("exit"));
                else
                    QVERIFY(state.order.indexOf("exit") < state.order.indexOf("view.destroy"));
            }
            if (state.scenario.startsWith("retained")) {
                QVERIFY(!state.windows.expired());
                QVERIFY(!state.retained->busy() && !state.retained->currentDialog());
                QVERIFY(!state.retained->windowFor(state.rootAddress));
                const auto before = state.notifications;
                qobject_cast<WindowManager *>(state.retained.get())->windowCleanupFailed();
                QCOMPARE(state.notifications, before); // run 返回前已断开接口订阅。
            }
            if (state.scenario == "cleanupSignal") QCOMPARE(state.notifications, 1);
            state.retained.reset();
            if (state.worker.isRunning()) { state.worker.quit(); state.worker.wait(); }
            QVERIFY(state.windows.expired());
            const auto before = state.counts();
            bootstrapper.reset();
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            QCoreApplication::processEvents();
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            QVERIFY(state.counts() == before);
            QVERIFY(QGuiApplication::allWindows().isEmpty());
        };
        Observations first;
        first.scenario = scenario;
        run(first);
        if (scenario == "sequential" || scenario.startsWith("recoverAfter")) {
            const auto before = first.counts();
            Observations second;
            second.scenario = "success";
            second.skipMappings = true;
            run(second);
            QCOMPARE(first.resolves, 1);
            QCOMPARE(first.creates, 1);
            QVERIFY(first.counts() == before);
        }
    }
};
QTEST_MAIN(BootstrapperProviderTests)
#include "tst_bootstrapper_provider.moc"
