#include <ViewModelComposition.h>
#include <QQmlEngine>
#include <QtTest>
#include <stdexcept>
#include <QRegularExpression>

static DetailViewModelFactory makeDetailFactory(
        std::shared_ptr<CounterService> service = std::make_shared<CounterService>())
{
    return [service] { return std::make_unique<DetailViewModel>(service); };
}

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
        auto shell = std::make_unique<ShellViewModel>(factory, makeDetailFactory(service));
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
    void navigationPreservesHomeAndSharesService()
    {
        auto service = std::make_shared<CounterService>();
        int homes = 0, details = 0;
        ShellViewModel shell([&] {
            ++homes;
            return std::make_unique<HomeViewModel>(service);
        }, [&] {
            ++details;
            return std::make_unique<DetailViewModel>(service);
        });
        auto *home = shell.home();
        QSignalSpy initialized(home, &ScreenViewModel::isInitializedChanged);
        QSignalSpy navigation(&shell, &ShellViewModel::navigationChanged);
        QVERIFY(!shell.canShowDetail());
        QVERIFY(!shell.showDetail());
        QCOMPARE(details, 0);
        shell.activate();
        home->add(3);
        QVERIFY(shell.canShowDetail());
        QVERIFY(shell.showDetail());
        auto *detail = shell.detail();
        QVERIFY(detail && detail->isActive() && !home->isActive());
        QCOMPARE(shell.home(), home);
        QCOMPARE(shell.items().size(), 2);
        QCOMPARE(detail->count(), 3);
        QCOMPARE(detail->message(), QStringLiteral("共享计数：3"));
        QCOMPARE(detail->parent(), &shell);
        QCOMPARE(QQmlEngine::objectOwnership(detail), QQmlEngine::CppOwnership);
        QSignalSpy detailCount(detail, &DetailViewModel::countChanged);
        QSignalSpy homeCount(home, &HomeViewModel::countChanged);
        service->add(1);
        QCOMPARE(detail->count(), 4);
        QCOMPARE(home->count(), 4);
        QCOMPARE(detailCount.count(), 1);
        QCOMPARE(homeCount.count(), 1);
        QVERIFY(!shell.showDetail());
        QCOMPARE(details, 1);
        shell.deactivate();
        QVERIFY(!shell.showDetail());
        QVERIFY(!detail->isActive());
        QVERIFY(!detail->goBack());
        shell.activate();
        QCOMPARE(shell.activeItem(), detail);
        QCOMPARE(shell.detail(), detail);
        QPointer<DetailViewModel> old = detail;
        QSignalSpy destroyed(detail, &QObject::destroyed);
        QVERIFY(detail->goBack());
        QVERIFY(!shell.detail() && old);
        QCOMPARE(shell.items().size(), 1);
        QCOMPARE(shell.activeItem(), home);
        QVERIFY(home->isActive() && !old->isActive());
        QVERIFY(!old->goBack());
        QVERIFY(shell.showDetail()); // 旧 Detail 尚未处理删除事件。
        QVERIFY(shell.detail() != old.data());
        QCOMPARE(shell.detail()->count(), 4);
        QCOMPARE(homes, 1);
        QCOMPARE(details, 2);
        QCOMPARE(initialized.count(), 1);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(!old);
        QCOMPARE(destroyed.count(), 1);
        QVERIFY(shell.detail() && shell.detail()->isActive());
        QVERIFY(navigation.count() > 0);
    }

    void detailFactoryFailuresKeepCurrentPage()
    {
        auto service = std::make_shared<CounterService>();
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, DetailViewModel{nullptr});
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                                (ShellViewModel{[service] { return std::make_unique<HomeViewModel>(service); }, {}}));
        int mode = 0;
        QObject parent;
        QPointer<DetailViewModel> invalid;
        ShellViewModel shell([service] { return std::make_unique<HomeViewModel>(service); }, [&]() -> std::unique_ptr<DetailViewModel> {
            if (mode == 0)
                return {};
            if (mode == 1)
                throw std::runtime_error("Detail 工厂异常");
            auto result = std::make_unique<DetailViewModel>(service);
            invalid = result.get();
            if (mode == 2)
                result->setParent(&parent);
            else
                result->activate();
            return result;
        });
        shell.activate();
        auto *home = shell.home();
        home->add(2);
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, shell.showDetail());
        mode = 1;
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, shell.showDetail());
        mode = 2;
        QTest::ignoreMessage(QtWarningMsg, "Collection.OneActive：接管对象必须无父对象且位于同一线程");
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, shell.showDetail());
        QVERIFY(!invalid && parent.children().isEmpty());
        mode = 3;
        QTest::ignoreMessage(QtWarningMsg, "Collection.OneActive：不能接管已经激活的 Screen");
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, shell.showDetail());
        QVERIFY(!invalid);
        QCOMPARE(shell.activeItem(), home);
        QVERIFY(home->isActive() && shell.canShowDetail());
        QCOMPARE(shell.items().size(), 1);
        QCOMPARE(home->count(), 2);
    }

    void missingPagesRecoverOnlyOnRequestedNavigation()
    {
        auto shell = buildShell();
        shell->activate();
        shell->home()->add(4);
        QVERIFY(shell->showDetail());
        auto *detail = shell->detail();
        delete shell->home(); // 非当前 Home 失效，不打断 Detail。
        QVERIFY(!shell->home());
        QCOMPARE(shell->activeItem(), detail);
        QVERIFY(detail->tryClose()); // 统一关闭入口先补入 Home，再关闭 Detail。
        QCOMPARE(shell->activeItem(), shell->home());
        QCOMPARE(shell->home()->count(), 4);
        QVERIFY(shell->showDetail());
        delete shell->detail();
        QVERIFY(!shell->activeItem());
        QVERIFY(shell->home() && !shell->home()->isActive());
        shell->activate();
        QVERIFY(!shell->activeItem()); // 活动 Shell 的重复激活仍无操作。
        shell->deactivate();
        shell->activate();
        QCOMPARE(shell->activeItem(), shell->home());
        QVERIFY(shell->home()->isActive());
    }

    void failedHomeRecoveryPreservesDetail()
    {
        auto service = std::make_shared<CounterService>();
        bool failHome = false;
        ShellViewModel shell([&]() -> std::unique_ptr<HomeViewModel> {
            return failHome ? nullptr : std::make_unique<HomeViewModel>(service);
        }, makeDetailFactory(service));
        shell.activate();
        QVERIFY(shell.showDetail());
        auto *detail = shell.detail();
        delete shell.home();
        failHome = true;
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, detail->tryClose());
        QCOMPARE(shell.activeItem(), detail);
        QVERIFY(detail->isActive() && shell.isActive());
        QCOMPARE(shell.items().size(), 1);
        failHome = false;
        QVERIFY(detail->tryClose());
        QVERIFY(shell.home() && shell.home()->isActive());
    }

    void detailReturnFailureAndRetry_data()
    {
        QTest::addColumn<int>("failure");
        QTest::newRow("nullHome") << 1;
        QTest::newRow("rejectedHome") << 2;
        QTest::newRow("factoryException") << 3;
        QTest::newRow("unknownException") << 4;
    }
    void detailReturnFailureAndRetry()
    {
        QFETCH(int, failure);
        auto service = std::make_shared<CounterService>();
        int mode = 0;
        ShellViewModel shell([&]() -> std::unique_ptr<HomeViewModel> {
            if (mode == 1) return nullptr;
            if (mode == 3) throw std::runtime_error("Home 工厂失败");
            if (mode == 4) throw 42;
            auto home = std::make_unique<HomeViewModel>(service);
            if (mode == 2) home->activate();
            return home;
        }, makeDetailFactory(service));
        QVERIFY(!shell.tryClose());
        shell.activate();
        QVERIFY(!shell.tryClose());
        QVERIFY(shell.isActive() && shell.home()->isActive());
        shell.home()->add(3);
        QVERIFY(shell.showDetail());
        auto *detail = shell.detail();
        QVERIFY(detail->metaObject()->indexOfMethod("goBack()") >= 0);
        QCOMPARE(detail->metaObject()->indexOfMethod("tryClose()"), -1);
        delete shell.home();
        mode = failure;
        QSignalSpy items(&shell, &ShellViewModel::itemsChanged);
        QSignalSpy selected(&shell, &ShellViewModel::activeItemChanged);
        QSignalSpy parent(detail, &ScreenViewModel::parentViewModelChanged);
        QSignalSpy active(detail, &ScreenViewModel::isActiveChanged);
        const auto expectRejectionWarning = [&] {
            if (failure == 2)
                QTest::ignoreMessage(QtWarningMsg, "Collection.OneActive：不能接管已经激活的 Screen");
        };
        expectRejectionWarning();
        if (failure <= 2) QVERIFY_THROWS_EXCEPTION(std::invalid_argument, detail->tryClose());
        else if (failure == 3) QVERIFY_THROWS_EXCEPTION(std::runtime_error, detail->tryClose());
        else QVERIFY_THROWS_EXCEPTION(int, detail->tryClose());
        expectRejectionWarning();
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("Detail 返回失败：.*"));
        bool returned = true;
        QVERIFY(QMetaObject::invokeMethod(detail, "goBack", Q_RETURN_ARG(bool, returned)));
        QVERIFY(!returned);
        QCOMPARE(shell.activeItem(), detail);
        QCOMPARE(shell.items().size(), 1);
        QCOMPARE(detail->parentViewModel(), &shell);
        QVERIFY(detail->isActive());
        QCOMPARE(items.count(), 0);
        QCOMPARE(selected.count(), 0);
        QCOMPARE(parent.count(), 0);
        QCOMPARE(active.count(), 0);
        mode = 0;
        QPointer<DetailViewModel> old = detail;
        QVERIFY(detail->goBack());
        QVERIFY(shell.home()->isActive() && !shell.detail());
        QCOMPARE(shell.home()->count(), 3);
        QCOMPARE(shell.activeItem(), shell.home());
        QVERIFY(old && !old->parentViewModel() && !old->goBack());
        QCOMPARE(items.count(), 2); // 补入 Home、移除 Detail。
        QCOMPARE(selected.count(), 1);
        QCOMPARE(parent.count(), 1);
        QCOMPARE(active.count(), 1);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(!old);
    }

    void shellProtocolOnlyRecoversForCurrentDetailClose()
    {
        auto service = std::make_shared<CounterService>();
        int homes = 0;
        ShellViewModel shell([&] { ++homes; return std::make_unique<HomeViewModel>(service); },
                             makeDetailFactory(service));
        shell.activate();
        QVERIFY(shell.showDetail());
        auto *detail = shell.detail();
        delete shell.home();
        IConductor *api = &shell;
        ViewModelBase foreign;
        QVERIFY(!api->deactivateItem(nullptr, true));
        QVERIFY(!api->deactivateItem(&foreign, true));
        QVERIFY(api->deactivateItem(detail, false));
        QCOMPARE(shell.activeItem(), detail);
        QVERIFY(!detail->isActive() && !detail->goBack());
        QCOMPARE(homes, 1);
        QVERIFY(shell.activateItem(detail));
        QVERIFY(detail->goBack());
        QCOMPARE(homes, 2);
        QVERIFY(shell.showDetail());
        detail = shell.detail();
        QVERIFY(shell.activateItem(shell.home())); // 留存的非当前 Detail 也能显式关闭。
        QVERIFY(!detail->goBack());
        QVERIFY(api->deactivateItem(detail, true));
        QCOMPARE(homes, 2);
        QCOMPARE(shell.activeItem(), shell.home());
    }

    void closeAllNavigationPagesAndServiceOnce()
    {
        auto service = std::make_shared<CounterService>();
        std::weak_ptr<CounterService> weak = service;
        QSignalSpy serviceDestroyed(service.get(), &QObject::destroyed);
        auto shell = std::make_unique<ShellViewModel>(
            [service] { return std::make_unique<HomeViewModel>(service); },
            makeDetailFactory(service));
        service.reset();
        shell->activate();
        shell->home()->add(5);
        QVERIFY(shell->showDetail());
        QPointer<HomeViewModel> oldHome = shell->home();
        QPointer<DetailViewModel> oldDetail = shell->detail();
        QSignalSpy homeDestroyed(oldHome.data(), &QObject::destroyed);
        QSignalSpy detailDestroyed(oldDetail.data(), &QObject::destroyed);
        shell->deactivate(true);
        QVERIFY(shell->items().isEmpty() && !shell->home() && !shell->detail());
        QVERIFY(oldHome && oldDetail);
        shell->activate();
        QCOMPARE(shell->home()->count(), 5);
        QVERIFY(shell->showDetail());
        QPointer<HomeViewModel> currentHome = shell->home();
        QPointer<DetailViewModel> currentDetail = shell->detail();
        shell.reset();
        QVERIFY(!oldHome && !oldDetail && !currentHome && !currentDetail);
        QVERIFY(weak.expired());
        QCOMPARE(homeDestroyed.count(), 1);
        QCOMPARE(detailDestroyed.count(), 1);
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
        }, makeDetailFactory(service));
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
