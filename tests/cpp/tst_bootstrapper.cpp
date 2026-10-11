#include "../support/DialogWindowSupport.h"
#include "../support/TestWindowManager.h"
#include <AppBootstrapper.h>
#include <ViewModelComposition.h>
#include <CaliburnMicroQt/ViewRegistry.h>
#include <CaliburnMicroQt/WindowManager.h>
#include <CaliburnMicroQt/ConfirmActionViewModel.h>
#include <QQmlComponent>
#include <QGuiApplication>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <QtTest>
#include <stdexcept>

Q_IMPORT_QML_PLUGIN(CaliburnMicroQtPlugin)
Q_IMPORT_QML_PLUGIN(CaliburnExampleModulePlugin)


static HomeViewModelFactory makeHomeFactory(
        std::shared_ptr<CounterService> service = std::make_shared<CounterService>())
{
    return [service] { return std::make_unique<HomeViewModel>(service, testWindows()); };
}

struct Observation {
    QStringList order;
    QPointer<ShellViewModel> shell;
    QPointer<HomeViewModel> home;
    QPointer<QQuickWindow> window;
    int factoryCalls = 0;
    int exitCalls = 0;
    int closeCalls = 0; // 非活动状态通知次数，与钩子次数分别统计。
    int closeHookCalls = 0;
    bool loopEntered = false;
    bool typedInjection = false;
    bool homeLoaded = false;
    bool aliveAtExit = false;
    bool activeAtExit = false;
    bool serviceInjected = false;
    bool dialogOpened = false;
    bool exitReturned = false;
    bool exitBeforeReturn = false;
    QPointer<IWindowManager> windows;
    std::shared_ptr<IWindowManager> retainedWindows;
};

static DetailViewModelFactory makeDetailFactory(
        std::shared_ptr<CounterService> service = std::make_shared<CounterService>())
{
    return [service] { return std::make_unique<DetailViewModel>(service, testWindows()); };
}

class FailingCloseShell : public ShellViewModel
{
public:
    explicit FailingCloseShell(Observation &observation)
        : ShellViewModel(makeHomeFactory(), makeDetailFactory()), m_observation(observation)
    {
        QQmlEngine::setObjectOwnership(home(), QQmlEngine::CppOwnership);
    }
protected:
    void onDeactivate(bool close) override
    {
        ++m_observation.closeHookCalls;
        m_observation.order << "close.hook";
        ShellViewModel::onDeactivate(close);
        throw std::runtime_error("测试关闭异常");
    }
private:
    Observation &m_observation;
};

class TestBootstrapper : public BootstrapperBase
{
public:
    TestBootstrapper(QGuiApplication &app, QString scenario, Observation &observation)
        : BootstrapperBase(app), m_scenario(std::move(scenario)), m_observation(observation) {}

protected:
    bool Configure() override
    {
        m_observation.order << "configure";
        if (m_scenario == "configureFailure")
            return false;
        if (m_scenario == "emptyFactory")
            return RegisterRootFactory<ShellViewModel>({});
        if (m_scenario != "missingFactory") {
            if (!RegisterRootFactory<ShellViewModel>([this](std::shared_ptr<IWindowManager> windows) {
                m_observation.serviceInjected = bool(windows);
                m_observation.windows = windows.get();
                if (m_scenario == "retainedService") m_observation.retainedWindows = windows;
                m_windows = qobject_cast<WindowManager *>(windows.get());
                if (m_scenario == "hostFailure") {
                    m_conflictingRoot = std::make_unique<ShellViewModel>(makeHomeFactory(), makeDetailFactory());
                    if (!m_windows->showWindow(QVariant::fromValue(m_conflictingRoot.get())))
                        return std::unique_ptr<ShellViewModel>{};
                }
                return CreateShell(std::move(windows));
            }))
                return false;
        }
        if (m_scenario == "missingMapping")
            return true;

        return RegisterMappings();
    }

    void OnStartup() override
    {
        m_observation.order << "startup";
        if (m_scenario == "startupException") throw std::runtime_error("测试启动异常");
        if (m_scenario == "lateRegistration") {
            QTest::ignoreMessage(QtWarningMsg, "ViewRegistry：配置已冻结，拒绝登记");
            if (ViewRegistry::registerView<ShellViewModel>(QUrl(QStringLiteral("qrc:/qt/qml/CaliburnExample/views/ShellView.qml"))))
                return;
        }
        if (m_scenario == "noRoot")
            return;
        const bool displayed = m_scenario == "explicitOptions"
            ? DisplayRootViewFor<ShellViewModel>(RootViewOptions{})
            : DisplayRootViewFor<ShellViewModel>();
        if (m_scenario == "ignoredLoadFailure")
            return;
        if (!displayed)
            return;
        if (m_scenario == "duplicateRoot") {
            (void)DisplayRootViewFor<ShellViewModel>();
            return;
        }

        QTimer::singleShot(0, this, [this] {
            m_observation.loopEntered = true;
            for (auto *window : QGuiApplication::allWindows()) {
                auto *root = qobject_cast<QQuickWindow *>(window);
                if (!root || root->property("viewModel").value<ShellViewModel *>() != m_observation.shell)
                    continue;
                m_observation.window = root;
                if (m_scenario == "hiddenWindow") QVERIFY(root->isVisible());
                if (m_scenario == "injectedService" || m_scenario == "explicitOptions") {
                    m_observation.home->add(2);
                    m_observation.home->reset();
                    m_observation.dialogOpened = m_windows->currentDialog()
                        && dialogControl(root, QStringLiteral("dialogAccept"));
                    if (auto *dialog = m_windows->currentDialog()) dialog->tryClose();
                }
                m_observation.typedInjection = root->property("viewModel").metaType()
                    == QMetaType::fromType<ShellViewModel *>();
                auto *host = root->findChild<QQuickItem *>(QStringLiteral("homeHost"));
                auto *item = host ? host->property("item").value<QQuickItem *>() : nullptr;
                m_observation.homeLoaded = item
                    && item->property("viewModel").value<HomeViewModel *>() == m_observation.home;
                if (item)
                    connect(item, &QObject::destroyed, this, [this] { m_observation.order << "homeView.destroy"; });
                connect(root, &QObject::destroyed, this, [this] { m_observation.order << "view.destroy"; });
            }
            if (m_scenario == "windowCloseException") {
                m_observation.window->close();
                return;
            }
            if (m_scenario == "externalCloseException") {
                try { m_observation.shell->deactivate(true); }
                catch (const std::runtime_error &) {}
            }
            QCoreApplication::exit(m_scenario == "exitCode" ? 7 : 0);
            m_observation.exitReturned = true;
        });
    }

    void OnExit() override
    {
        ++m_observation.exitCalls;
        m_observation.exitBeforeReturn = !m_observation.exitReturned;
        m_observation.order << "exit";
        m_observation.aliveAtExit = bool(m_observation.shell); // 根对象在 OnExit 时仍有效，Home 可已延迟释放。
        m_observation.activeAtExit = m_observation.shell && m_observation.shell->isActive();
        if (m_scenario == "exitException")
            throw std::runtime_error("测试退出异常");
    }

private:
    bool RegisterMappings()
    {
        QUrl url(QStringLiteral("qrc:/qt/qml/CaliburnExample/views/ShellView.qml"));
        if (m_scenario == "hiddenWindow")
            url = QUrl(QStringLiteral("qrc:/tests/fixtures/HiddenShell.qml"));
        if (m_scenario == "missingResource" || m_scenario == "ignoredLoadFailure")
            url = QUrl(QStringLiteral("qrc:/tests/missing.qml"));
        if (m_scenario == "nonWindow")
            url = QUrl(QStringLiteral("qrc:/tests/fixtures/NonVisual.qml"));
        if (m_scenario == "remoteView")
            url = QUrl(QStringLiteral("https://example.invalid/ShellView.qml"));

        if (!ViewRegistry::registerView<ShellViewModel>(url))
            return false;
        if (m_scenario == "configureQuery") {
            ShellViewModel probe(makeHomeFactory(), makeDetailFactory());
            if (ViewRegistry::viewUrl(&probe) != url)
                return false;
        }
        return ViewRegistry::registerView<HomeViewModel>(QUrl(QStringLiteral("qrc:/qt/qml/CaliburnExample/views/HomeView.qml")))
            && ViewRegistry::registerView<DetailViewModel>(QUrl(QStringLiteral("qrc:/qt/qml/CaliburnExample/views/DetailView.qml")));
    }
    std::unique_ptr<ShellViewModel> CreateShell(std::shared_ptr<IWindowManager> windows)
    {
        ++m_observation.factoryCalls;
        m_observation.order << "factory";
        if (m_scenario == "factoryException")
            throw std::runtime_error("测试工厂异常");
        if (m_scenario == "nullRoot")
            return {};
        std::unique_ptr<ShellViewModel> shell;
        if (m_scenario == "closeException" || m_scenario == "windowCloseException" || m_scenario == "externalCloseException")
            shell = std::make_unique<FailingCloseShell>(m_observation);
        else
            shell = buildShell(std::move(windows));
        m_observation.shell = shell.get();
        m_observation.home = shell->home();
        connect(shell.get(), &QObject::destroyed, this, [this] { m_observation.order << "shell.destroy"; });
        connect(shell->home(), &QObject::destroyed, this, [this] { m_observation.order << "home.destroy"; });
        connect(shell.get(), &ScreenViewModel::isActiveChanged, this, [this] {
            if (!m_observation.shell->isActive()) {
                ++m_observation.closeCalls;
                m_observation.order << "close";
            }
        });
        return shell;
    }

    QPointer<WindowManager> m_windows;
    std::unique_ptr<ShellViewModel> m_conflictingRoot;
    QString m_scenario;
    Observation &m_observation;
};

class WindowCloseBootstrapper : public BootstrapperBase
{
public:
    using BootstrapperBase::BootstrapperBase;
    std::function<void()> exercise;
    std::function<void()> exitCheck;
    std::shared_ptr<WindowManager> windows;
    QPointer<ShellViewModel> shell;
    QPointer<QQuickWindow> window;
    int homesCreated = 0;
    int closes = 0;
    int exits = 0;
    bool aliveAtExit = false;
    bool badConfirmation = false;
protected:
    bool Configure() override
    {
        if (badConfirmation && !ViewRegistry::registerView<ConfirmActionViewModel>(QUrl("qrc:/tests/missing-confirm.qml")))
            return false;
        return RegisterRootFactory<ShellViewModel>([this](std::shared_ptr<IWindowManager> service) {
            windows = std::static_pointer_cast<WindowManager>(service);
            auto counter = std::make_shared<CounterService>();
            HomeViewModelFactory home = [this, counter, service] {
                ++homesCreated;
                return std::make_unique<HomeViewModel>(counter, service);
            };
            DetailViewModelFactory detail = [counter, service] {
                return std::make_unique<DetailViewModel>(counter, service);
            };
            auto root = std::make_unique<ShellViewModel>(home, detail);
            shell = root.get();
            connect(root.get(), &ScreenViewModel::attemptingDeactivation, this, [this](bool close) {
                if (close) ++closes;
            });
            return root;
        }) && ViewRegistry::registerView<ShellViewModel>(QUrl("qrc:/qt/qml/CaliburnExample/views/ShellView.qml"))
            && ViewRegistry::registerView<HomeViewModel>(QUrl("qrc:/qt/qml/CaliburnExample/views/HomeView.qml"))
            && ViewRegistry::registerView<DetailViewModel>(QUrl("qrc:/qt/qml/CaliburnExample/views/DetailView.qml"));
    }
    void OnStartup() override
    {
        if (!DisplayRootViewFor<ShellViewModel>()) return;
        for (auto *candidate : QGuiApplication::allWindows()) {
            auto *quick = qobject_cast<QQuickWindow *>(candidate);
            if (quick && quick->property("viewModel").value<ShellViewModel *>() == shell)
                window = quick;
        }
        QTimer::singleShot(0, this, [this] { exercise(); });
    }
    void OnExit() override
    {
        if (exitCheck) exitCheck();
        ++exits;
        aliveAtExit = shell && window && !shell->isActive();
    }
};

class DefaultStartupBootstrapper : public BootstrapperBase
{
public:
    using BootstrapperBase::BootstrapperBase;
protected:
    bool Configure() override { return true; }
};

class BootstrapperTests : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        // 离屏测试使用 Basic，避免 macOS 原生控件样式依赖 NSView。
        QQuickStyle::setStyle(QStringLiteral("Basic"));
    }

    void startupAndCleanup_data()
    {
        QTest::addColumn<QString>("scenario");
        QTest::addColumn<int>("exitCode");
        QTest::addColumn<bool>("entersLoop");
        QTest::addColumn<int>("factoryCalls");
        QTest::addColumn<int>("closeCalls");
        QTest::addColumn<bool>("aliveAtExit");
        QTest::newRow("injectedService") << QString("injectedService") << 0 << true << 1 << 1 << true;
        QTest::newRow("explicitOptions") << QString("explicitOptions") << 0 << true << 1 << 1 << true;
        QTest::newRow("emptyFactory") << QString("emptyFactory") << 1 << false << 0 << 0 << false;
        // 服务已有自建普通窗口时，在激活根 VM 前拒绝显示。
        QTest::newRow("hostFailure") << QString("hostFailure") << 1 << false << 1 << 0 << true;
        QTest::newRow("hiddenWindow") << QString("hiddenWindow") << 0 << true << 1 << 1 << true;
        QTest::newRow("retainedService") << QString("retainedService") << 0 << true << 1 << 1 << true;
        QTest::newRow("success") << QString("success") << 0 << true << 1 << 1 << true;
        QTest::newRow("configureQuery") << QString("configureQuery") << 0 << true << 1 << 1 << true;
        QTest::newRow("lateRegistration") << QString("lateRegistration") << 0 << true << 1 << 1 << true;
        QTest::newRow("exitCode") << QString("exitCode") << 7 << true << 1 << 1 << true;
        QTest::newRow("configureFailure") << QString("configureFailure") << 1 << false << 0 << 0 << false;
        QTest::newRow("missingFactory") << QString("missingFactory") << 1 << false << 0 << 0 << false;
        QTest::newRow("factoryException") << QString("factoryException") << 1 << false << 1 << 0 << false;
        QTest::newRow("nullRoot") << QString("nullRoot") << 1 << false << 1 << 0 << false;
        QTest::newRow("missingMapping") << QString("missingMapping") << 1 << false << 1 << 0 << true;
        QTest::newRow("remoteView") << QString("remoteView") << 1 << false << 1 << 0 << true;
        QTest::newRow("missingResource") << QString("missingResource") << 1 << false << 1 << 1 << true;
        QTest::newRow("nonWindow") << QString("nonWindow") << 1 << false << 1 << 1 << true;
        QTest::newRow("noRoot") << QString("noRoot") << 1 << false << 0 << 0 << false;
        QTest::newRow("ignoredLoadFailure") << QString("ignoredLoadFailure") << 1 << false << 1 << 1 << true;
        QTest::newRow("duplicateRoot") << QString("duplicateRoot") << 1 << false << 1 << 1 << true;
        QTest::newRow("exitException") << QString("exitException") << 1 << true << 1 << 1 << true;
        QTest::newRow("closeException") << QString("closeException") << 1 << true << 1 << 1 << true;
        QTest::newRow("startupException") << QString("startupException") << 1 << false << 0 << 0 << false;
        QTest::newRow("windowCloseException") << QString("windowCloseException") << 1 << true << 1 << 1 << true;
        QTest::newRow("externalCloseException") << QString("externalCloseException") << 0 << true << 1 << 1 << true;
    }

    void startupAndCleanup()
    {
        QFETCH(QString, scenario);
        QFETCH(int, exitCode);
        QFETCH(bool, entersLoop);
        QFETCH(int, factoryCalls);
        QFETCH(int, closeCalls);
        QFETCH(bool, aliveAtExit);
        Observation observation;
        {
            TestBootstrapper bootstrapper(*qobject_cast<QGuiApplication *>(qApp), scenario, observation);
            QCOMPARE(observation.factoryCalls, 0);
            QCOMPARE(bootstrapper.Run(), exitCode);
            QCOMPARE(observation.loopEntered, entersLoop);
            QCOMPARE(observation.factoryCalls, factoryCalls);
            QCOMPARE(observation.closeCalls, closeCalls);
            QCOMPARE(observation.closeHookCalls, int(scenario == "closeException" || scenario == "windowCloseException"
                                                   || scenario == "externalCloseException"));
            QCOMPARE(observation.aliveAtExit, aliveAtExit);
            QVERIFY(!observation.activeAtExit);
            QCOMPARE(observation.exitCalls, 1);
            if (factoryCalls > 0) {
                QVERIFY(observation.serviceInjected);
                if (scenario == "retainedService") {
                    QVERIFY(observation.windows);
                    QVERIFY(!observation.windows->busy());
                    QVERIFY(!observation.windows->currentDialog());
                    QVERIFY(QGuiApplication::allWindows().isEmpty());
                    observation.retainedWindows.reset();
                }
                QVERIFY(!observation.windows);
                if (scenario == "injectedService" || scenario == "explicitOptions")
                    QVERIFY(observation.dialogOpened);
            }
            QVERIFY(!observation.shell && !observation.home && !observation.window);
            if (scenario == "configureFailure" || scenario == "emptyFactory")
                QVERIFY(ViewRegistry::registerView<HomeViewModel>(QUrl(QStringLiteral("qrc:/qt/qml/CaliburnExample/views/HomeView.qml"))));
            if (entersLoop) {
                QVERIFY(observation.exitBeforeReturn);
                if (scenario != "windowCloseException") QVERIFY(observation.exitReturned);
                QVERIFY(observation.typedInjection);
                QVERIFY(observation.homeLoaded);
                QVERIFY(observation.order.indexOf("close") < observation.order.indexOf("exit"));
                QVERIFY(observation.order.indexOf("exit") < observation.order.indexOf("view.destroy"));
                QVERIFY(observation.order.indexOf("view.destroy") < observation.order.indexOf("shell.destroy"));
                QVERIFY(observation.order.indexOf("homeView.destroy") >= 0);
                QVERIFY(observation.order.indexOf("homeView.destroy") < observation.order.indexOf("home.destroy"));
                QCOMPARE(observation.order.mid(0, 3), QStringList({"configure", "startup", "factory"}));
            }
            QCOMPARE(bootstrapper.Run(), 1);
            QCOMPARE(observation.exitCalls, 1);
        }
        QCOMPARE(observation.exitCalls, 1);
    }

    void defaultStartup()
    {
        DefaultStartupBootstrapper bootstrapper(*qobject_cast<QGuiApplication *>(qApp));
        QCOMPARE(bootstrapper.Run(), 1); // 默认空钩子不能绕过必须显示根窗口的约束。
    }

    void rootWindowClose_data()
    {
        QTest::addColumn<QString>("scenario");
        for (const auto *name : {"home", "accept", "cancel", "escape", "rootTryClose", "modelClose",
                                 "ordinaryDeactivate", "quit", "forcedExit", "forcedExitDialog", "inactiveDetail", "busy", "missingHome", "showFailure"})
            QTest::newRow(name) << QString::fromLatin1(name);
    }

    void rootWindowClose()
    {
        QFETCH(QString, scenario);
        WindowCloseBootstrapper bootstrapper(*qobject_cast<QGuiApplication *>(qApp));
        bootstrapper.badConfirmation = scenario == "showFailure";
        bool exercised = false;
        int prompts = 0;
        bool viewBeforeVm = false;
        QTimer deadline;
        deadline.setSingleShot(true);
        connect(&deadline, &QTimer::timeout, this, [] { QCoreApplication::exit(9); });
        deadline.start(10000);
        bootstrapper.exercise = [&] {
            auto *shell = bootstrapper.shell.data();
            auto *window = bootstrapper.window.data();
            auto *windows = bootstrapper.windows.get();
            QVERIFY(shell && window && windows);
            QVERIFY(QTest::qWaitForWindowExposed(window));
            connect(windows, &IWindowManager::currentDialogChanged, &bootstrapper, [&, windows] {
                if (windows->currentDialog()) ++prompts;
            });
            if (scenario != "home") shell->showDetail();
            QPointer<DetailViewModel> detail = shell->detail();
            if (detail) {
                auto *host = window->findChild<QQuickItem *>("homeHost");
                QPointer<QQuickItem> view = host ? host->property("item").value<QQuickItem *>() : nullptr;
                QVERIFY(view);
                connect(detail, &QObject::destroyed, &bootstrapper, [&, view] { viewBeforeVm = !view; });
            }
            if (scenario == "home") { window->close(); exercised = true; return; }
            if (scenario == "modelClose") { shell->deactivate(true); exercised = true; return; }
            if (scenario == "forcedExit") { exercised = true; QCoreApplication::exit(0); return; }
            if (scenario == "forcedExitDialog") {
                auto future = windows->showDialogAsync(std::make_unique<ConfirmActionViewModel>(
                    ConfirmationRequest{"退出时仍在展示", "测试强制清理"}), shell);
                QPointer<QQuickWindow> dialog = dialogWindow(window);
                QVERIFY(dialog && windows->busy());
                connect(windows, &IWindowManager::busyChanged, &bootstrapper, [&, future, dialog, windows] {
                    if (!windows->busy()) {
                        QVERIFY(!dialog);
                        // Future 在清理通知之后完成，在 OnExit 中继续检查。
                    }
                });
                bootstrapper.exitCheck = [future, dialog, windows] {
                    QVERIFY(future.isFinished()); QCOMPARE(future.result(), DialogResult{});
                    QVERIFY(!dialog && !windows->busy());
                };
                exercised = true; QCoreApplication::exit(0); return;
            }
            if (scenario == "showFailure") {
                window->close();
                QCoreApplication::processEvents();
                QTRY_VERIFY(!windows->busy());
                QVERIFY(window->isVisible() && detail->isActive());
                QCOMPARE(bootstrapper.closes, 0);
                exercised = true;
                QCoreApplication::exit(0);
                return;
            }
            if (scenario == "ordinaryDeactivate") {
                shell->deactivate(false);
                QCoreApplication::processEvents();
                QVERIFY(window->isVisible()); QCOMPARE(prompts, 0);
                shell->activate();
            }
            if (scenario == "inactiveDetail") shell->activateItem(shell->home());
            if (scenario == "missingHome") {
                shell->closeItem(shell->home());
                QVERIFY(!shell->home());
            }
            if (scenario == "busy") {
                auto future = windows->showDialogAsync(std::make_unique<ConfirmActionViewModel>(
                    ConfirmationRequest{"已有弹窗", "测试"}), shell);
                QVERIFY(windows->busy());
                window->close();
                QCoreApplication::processEvents();
                QVERIFY(window->isVisible() && shell->isActive());
                QCOMPARE(bootstrapper.closes, 0);
                windows->currentDialog()->tryClose();
                QTRY_VERIFY(future.isFinished());
            }
            if (scenario == "rootTryClose") shell->tryClose();
            else if (scenario == "quit") QCoreApplication::quit();
            else window->close();
            QTRY_VERIFY(windows->busy());
            auto *confirmation = qobject_cast<ConfirmActionViewModel *>(windows->currentDialog());
            QVERIFY(confirmation);
            QCOMPARE(confirmation->title(), QString("离开详情"));
            QCOMPARE(confirmation->message(), QString("确定离开当前详情吗？"));
            if (scenario == "cancel" || scenario == "escape") {
                if (scenario == "cancel") confirmation->cancel();
                else QTest::keyClick(dialogWindow(window), Qt::Key_Escape);
                QTRY_VERIFY(!windows->busy());
                QVERIFY(window->isVisible() && detail && detail->isActive());
                QCOMPARE(shell->activeItem(), detail.data());
                QCOMPARE(bootstrapper.closes, 0);
                QCOMPARE(bootstrapper.homesCreated, 1);
                QVERIFY(!window->close());
                QTRY_VERIFY(windows->busy());
                confirmation = qobject_cast<ConfirmActionViewModel *>(windows->currentDialog());
                QVERIFY(confirmation);
            }
            exercised = true;
            confirmation->accept();
        };
        QCOMPARE(bootstrapper.Run(), 0);
        deadline.stop();
        QVERIFY(exercised);
        QCOMPARE(bootstrapper.closes, 1);
        QCOMPARE(bootstrapper.exits, 1);
        QVERIFY(bootstrapper.aliveAtExit);
        QVERIFY(!bootstrapper.shell && !bootstrapper.window);
        QCOMPARE(bootstrapper.homesCreated, 1);
        if (scenario != "home") QVERIFY(viewBeforeVm);
        if (scenario == "home" || scenario == "modelClose" || scenario == "forcedExit") QCOMPARE(prompts, 0);
        else if (scenario == "forcedExitDialog") QCOMPARE(prompts, 1);
        else if (scenario != "showFailure")
            QCOMPARE(prompts, (scenario == "cancel" || scenario == "escape" || scenario == "busy") ? 2 : 1);
    }

    void exampleStartup()
    {
        AppBootstrapper bootstrapper(*qobject_cast<QGuiApplication *>(qApp));
        bool shellActive = false;
        bool homeActive = false;
        bool detailLoaded = false;
        QPointer<ShellViewModel> shell;
        QPointer<QQuickWindow> window;
        QTimer::singleShot(0, &bootstrapper, [&] {
            for (auto *candidate : QGuiApplication::allWindows()) {
                auto *root = qobject_cast<QQuickWindow *>(candidate);
                if (!root)
                    continue;
                shell = root->property("viewModel").value<ShellViewModel *>();
                if (!shell)
                    continue;
                window = root;
                shellActive = shell->isInitialized() && shell->isActive();
                homeActive = shell->home()->isInitialized() && shell->home()->isActive();
                shell->home()->add(2);
                shell->showDetail();
                if (shell->detail()) {
                    auto *host = root->findChild<QQuickItem *>(QStringLiteral("homeHost"));
                    auto *item = host ? host->property("item").value<QQuickItem *>() : nullptr;
                    detailLoaded = item && shell->detail()->count() == 2
                        && item->property("viewModel").value<DetailViewModel *>() == shell->detail();
                }
                // 验证从 Detail 关闭窗口也能清理集合，而非仅测试直接 exit。
                root->close();
                QTimer::singleShot(0, &bootstrapper, [root] {
                    auto *host = dialogWindow(root);
                    auto *manager = host ? qobject_cast<IWindowManager *>(root->property("_caliburnWindowManager").value<QObject *>()) : nullptr;
                    auto *confirmation = manager
                        ? qobject_cast<ConfirmActionViewModel *>(manager->currentDialog()) : nullptr;
                    QVERIFY(confirmation);
                    confirmation->accept();
                });
            }
        });
        QCOMPARE(bootstrapper.Run(), 0);
        QVERIFY(shellActive && homeActive && detailLoaded);
        QVERIFY(!shell && !window);
    }
};

QTEST_MAIN(BootstrapperTests)
#include "tst_bootstrapper.moc"
