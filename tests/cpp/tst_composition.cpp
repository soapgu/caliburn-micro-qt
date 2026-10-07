#include <ViewModelComposition.h>
#include <QQmlEngine>
#include <QtTest>
#include <stdexcept>

class CompositionTests : public QObject
{
    Q_OBJECT
private slots:
    void assembledTreeSurvivesInjectorAndOwnsChildren()
    {
        auto shell = buildShell();
        QVERIFY(shell && !shell->parent());
        auto *home = shell->home();
        QVERIFY(home);
        QCOMPARE(home->parent(), shell.get());
        QCOMPARE(home->thread(), shell->thread());
        QCOMPARE(QQmlEngine::objectOwnership(shell.get()), QQmlEngine::CppOwnership);
        QCOMPARE(QQmlEngine::objectOwnership(home), QQmlEngine::CppOwnership);
        QVERIFY(!shell->isInitialized() && !shell->isActive());
        QVERIFY(!home->isInitialized() && !home->isActive());
        QPointer<HomeViewModel> weak = home;
        QSignalSpy destroyed(home, &QObject::destroyed);
        home->add(2);
        auto second = buildShell();
        QVERIFY(second->home() != home);
        QCOMPARE(second->home()->count(), 0);
        QCOMPARE(home->count(), 2);
        shell.reset();
        QVERIFY(!weak);
        QCOMPARE(destroyed.count(), 1);
    }

    void rootLifecycleControlsHome()
    {
        auto shell = buildShell();
        auto *home = shell->home();
        QStringList order;
        connect(home, &ScreenViewModel::isInitializedChanged, this, [&] { order << "home.initialize"; });
        connect(shell.get(), &ScreenViewModel::isInitializedChanged, this, [&] { order << "shell.initialize"; });
        connect(home, &ScreenViewModel::isActiveChanged, this, [&] { order << (home->isActive() ? "home.activate" : "home.deactivate"); });
        connect(shell.get(), &ScreenViewModel::isActiveChanged, this, [&] { order << (shell->isActive() ? "shell.activate" : "shell.deactivate"); });
        shell->initialize();
        shell->initialize();
        QCOMPARE(order, QStringList({"shell.initialize"}));
        QVERIFY(!home->isInitialized());
        shell->activate();
        shell->activate();
        QCOMPARE(order, QStringList({"shell.initialize", "home.initialize", "home.activate", "shell.activate"}));
        home->add(2);
        shell->deactivate();
        shell->deactivate();
        shell->activate();
        QCOMPARE(shell->home(), home);
        QCOMPARE(home->count(), 2);
        shell->deactivate(true);
        shell->deactivate(true);
        QVERIFY(!shell->home());
        QVERIFY(!home->isActive());
    }

    void closedPageIsRecreated_data()
    {
        QTest::addColumn<bool>("flushDelete");
        QTest::newRow("before-delete-event") << false;
        QTest::newRow("after-delete-event") << true;
    }

    void closedPageIsRecreated()
    {
        QFETCH(bool, flushDelete);
        auto shell = buildShell();
        shell->activate();
        QPointer<HomeViewModel> old = shell->home();
        QSignalSpy destroyed(old.data(), &QObject::destroyed);
        old->add(4);
        shell->deactivate(true);
        QVERIFY(!shell->home());
        QVERIFY(old);
        if (flushDelete) {
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            QVERIFY(!old);
        }
        shell->activate();
        auto *next = shell->home();
        QVERIFY(next && next->isInitialized() && next->isActive());
        QCOMPARE(next->count(), 4);
        QCOMPARE(next->parent(), shell.get());
        QCOMPARE(QQmlEngine::objectOwnership(next), QQmlEngine::CppOwnership);
        if (old)
            QVERIFY(next != old.data());
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(!old);
        QCOMPARE(destroyed.count(), 1);
        QCOMPARE(shell->home(), next); // 旧销毁连接不会清空新项。
        next->reset();
        QCOMPARE(next->count(), 0);
    }

    void factoryAndServiceSurvivePageClosure()
    {
        auto service = std::make_shared<CounterService>();
        std::weak_ptr<CounterService> weakService = service;
        int calls = 0;
        HomeViewModelFactory factory = [service, &calls] {
            ++calls;
            return std::make_unique<HomeViewModel>(service);
        };
        auto shell = std::make_unique<ShellViewModel>(factory);
        QSignalSpy serviceDestroyed(service.get(), &QObject::destroyed);
        service.reset();
        factory = {};
        QCOMPARE(calls, 1);
        shell->deactivate(true); // 未初始化时关闭不清空。
        QCOMPARE(calls, 1);
        QVERIFY(shell->home());
        shell->activate();
        shell->home()->add(3);
        shell->deactivate();
        shell->activate();
        QCOMPARE(calls, 1);
        shell->deactivate(true);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(!weakService.expired());
        QVERIFY(!shell->home());
        shell->activate();
        QCOMPARE(calls, 2);
        QCOMPARE(shell->home()->count(), 3);
        QPointer<HomeViewModel> old = shell->home();
        shell->deactivate(true);
        shell->activate();
        QCOMPARE(calls, 3);
        QPointer<HomeViewModel> current = shell->home();
        shell.reset(); // 同时回收当前项和未处理 deleteLater 的旧项。
        QVERIFY(!old && !current);
        QVERIFY(weakService.expired());
        QCOMPARE(serviceDestroyed.count(), 1);
    }
    void rebuildingFailureLeavesEmptySelection()
    {
        int calls = 0;
        bool throwFromFactory = false;
        auto service = std::make_shared<CounterService>();
        ShellViewModel shell([&]() -> std::unique_ptr<HomeViewModel> {
            ++calls;
            if (calls == 1)
                return std::make_unique<HomeViewModel>(service);
            if (throwFromFactory)
                throw std::runtime_error("重建工厂异常");
            return {};
        });
        shell.activate();
        shell.home()->add(2);
        shell.deactivate(true);
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, shell.activate());
        QVERIFY(!shell.activeItem() && !shell.isActive());
        throwFromFactory = true;
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, shell.activate());
        QVERIFY(!shell.activeItem() && !shell.isActive());
        QCOMPARE(service->count(), 2);
    }
};

QTEST_GUILESS_MAIN(CompositionTests)
#include "tst_composition.moc"
