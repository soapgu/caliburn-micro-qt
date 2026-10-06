#include <AppBootstrapper.h>
#include <ViewModelComposition.h>
#include <CaliburnMicroQt/ViewRegistry.h>
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

struct Observation {
    QStringList order;
    QPointer<ShellViewModel> shell;
    QPointer<HomeViewModel> home;
    QPointer<QQuickWindow> window;
    int factoryCalls = 0;
    int exitCalls = 0;
    int closeCalls = 0;
    bool loopEntered = false;
    bool typedInjection = false;
    bool homeLoaded = false;
    bool aliveAtExit = false;
    bool activeAtExit = false;
};

class FailingCloseShell : public ShellViewModel
{
public:
    explicit FailingCloseShell(Observation &observation)
        : ShellViewModel(std::make_unique<HomeViewModel>()), m_observation(observation)
    {
        QQmlEngine::setObjectOwnership(home(), QQmlEngine::CppOwnership);
    }
protected:
    void onDeactivate(bool close) override
    {
        ++m_observation.closeCalls;
        m_observation.order << "close";
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
        if (m_scenario != "missingFactory") {
            if (!RegisterRootFactory<ShellViewModel>([this] { return CreateShell(); }))
                return false;
        }
        if (m_scenario == "missingMapping")
            return true;

        QUrl url(QStringLiteral("qrc:/qt/qml/CaliburnExample/views/ShellView.qml"));
        if (m_scenario == "missingResource" || m_scenario == "ignoredLoadFailure")
            url = QUrl(QStringLiteral("qrc:/tests/missing.qml"));
        if (m_scenario == "nonWindow")
            url = QUrl(QStringLiteral("qrc:/tests/fixtures/NonVisual.qml"));
        if (m_scenario == "remoteView")
            url = QUrl(QStringLiteral("https://example.invalid/ShellView.qml"));

        if (!ViewRegistry::registerView<ShellViewModel>(url))
            return false;
        if (m_scenario == "configureQuery") {
            ShellViewModel probe(std::make_unique<HomeViewModel>());
            if (ViewRegistry::viewUrl(&probe) != url)
                return false;
        }
        return ViewRegistry::registerView<HomeViewModel>(QUrl(QStringLiteral("qrc:/qt/qml/CaliburnExample/views/HomeView.qml")));
    }

    bool OnStartup() override
    {
        m_observation.order << "startup";
        if (m_scenario == "lateRegistration") {
            QTest::ignoreMessage(QtWarningMsg, "ViewRegistry：配置已冻结，拒绝登记");
            if (ViewRegistry::registerView<ShellViewModel>(QUrl(QStringLiteral("qrc:/qt/qml/CaliburnExample/views/ShellView.qml"))))
                return false;
        }
        if (m_scenario == "noRoot")
            return true;
        const bool displayed = DisplayRootViewFor<ShellViewModel>();
        if (m_scenario == "ignoredLoadFailure")
            return true;
        if (!displayed)
            return false;
        if (m_scenario == "duplicateRoot")
            return DisplayRootViewFor<ShellViewModel>();

        QTimer::singleShot(0, this, [this] {
            m_observation.loopEntered = true;
            for (auto *window : QGuiApplication::allWindows()) {
                auto *root = qobject_cast<QQuickWindow *>(window);
                if (!root || root->property("viewModel").value<ShellViewModel *>() != m_observation.shell)
                    continue;
                m_observation.window = root;
                m_observation.typedInjection = root->property("viewModel").metaType()
                    == QMetaType::fromType<ShellViewModel *>();
                auto *host = root->findChild<QQuickItem *>(QStringLiteral("homeHost"));
                auto *item = host ? host->property("item").value<QQuickItem *>() : nullptr;
                m_observation.homeLoaded = item
                    && item->property("viewModel").value<HomeViewModel *>() == m_observation.home;
                connect(root, &QObject::destroyed, this, [this] { m_observation.order << "view.destroy"; });
            }
            QCoreApplication::exit(m_scenario == "exitCode" ? 7 : 0);
        });
        return true;
    }

    void OnExit() override
    {
        ++m_observation.exitCalls;
        m_observation.order << "exit";
        m_observation.aliveAtExit = m_observation.shell && m_observation.home;
        m_observation.activeAtExit = m_observation.shell && m_observation.shell->isActive();
        if (m_scenario == "exitException")
            throw std::runtime_error("测试退出异常");
    }

private:
    std::unique_ptr<ShellViewModel> CreateShell()
    {
        ++m_observation.factoryCalls;
        m_observation.order << "factory";
        if (m_scenario == "factoryException")
            throw std::runtime_error("测试工厂异常");
        if (m_scenario == "nullRoot")
            return {};
        std::unique_ptr<ShellViewModel> shell;
        if (m_scenario == "closeException")
            shell = std::make_unique<FailingCloseShell>(m_observation);
        else
            shell = buildShell();
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

    QString m_scenario;
    Observation &m_observation;
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
            QCOMPARE(observation.aliveAtExit, aliveAtExit);
            QCOMPARE(observation.activeAtExit, scenario == "closeException");
            QCOMPARE(observation.exitCalls, 1);
            QVERIFY(!observation.shell && !observation.home && !observation.window);
            if (scenario == "configureFailure")
                QVERIFY(ViewRegistry::registerView<HomeViewModel>(QUrl(QStringLiteral("qrc:/qt/qml/CaliburnExample/views/HomeView.qml"))));
            if (entersLoop) {
                QVERIFY(observation.typedInjection);
                QVERIFY(observation.homeLoaded);
                QVERIFY(observation.order.indexOf("close") < observation.order.indexOf("exit"));
                QVERIFY(observation.order.indexOf("exit") < observation.order.indexOf("view.destroy"));
                QVERIFY(observation.order.indexOf("view.destroy") < observation.order.indexOf("shell.destroy"));
                QCOMPARE(observation.order.mid(0, 3), QStringList({"configure", "startup", "factory"}));
            }
            QCOMPARE(bootstrapper.Run(), 1);
            QCOMPARE(observation.exitCalls, 1);
        }
        QCOMPARE(observation.exitCalls, 1);
    }

    void exampleStartup()
    {
        AppBootstrapper bootstrapper(*qobject_cast<QGuiApplication *>(qApp));
        bool shellActive = false;
        bool homeActive = false;
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
                // 验证窗口关闭可以结束应用，而非仅测试直接 exit。
                root->close();
            }
        });
        QCOMPARE(bootstrapper.Run(), 0);
        QVERIFY(shellActive && homeActive);
        QVERIFY(!shell && !window);
    }
};

QTEST_MAIN(BootstrapperTests)
#include "tst_bootstrapper.moc"
