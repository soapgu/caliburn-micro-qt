#include <ViewModelComposition.h>
#include <QQmlEngine>
#include <QtTest>

class CompositionTests : public QObject
{
    Q_OBJECT
private slots:
    void assembledTreeSurvivesInjectorAndOwnsChildren()
    {
        // buildShell 返回时局部注入器已销毁，以下访问验证对象寿命独立于容器。
        auto shell = buildShell();
        QVERIFY(shell);
        QVERIFY(!shell->parent());
        auto *home = shell->home();
        QVERIFY(home);
        QCOMPARE(home->parent(), shell.get());
        QCOMPARE(home->thread(), shell->thread());
        QCOMPARE(QQmlEngine::objectOwnership(shell.get()), QQmlEngine::CppOwnership);
        QCOMPARE(QQmlEngine::objectOwnership(home), QQmlEngine::CppOwnership);
        QVERIFY(!shell->isInitialized());
        QVERIFY(!home->isInitialized());
        QVERIFY(!shell->isActive());
        QVERIFY(!home->isActive());
        QPointer<HomeViewModel> weak = home;
        QSignalSpy destroyed(home, &QObject::destroyed);
        home->add(2);
        QCOMPARE(home->count(), 2);
        auto second = buildShell();
        QVERIFY(second.get() != shell.get());
        QVERIFY(second->home() != home);
        QCOMPARE(second->home()->count(), 0);
        shell.reset();
        QVERIFY(!weak);
        QCOMPARE(destroyed.count(), 1);
    }

    void rootLifecycleControlsHome()
    {
        auto shell = buildShell();
        auto *home = shell->home();
        QSignalSpy shellInitialized(shell.get(), &ScreenViewModel::isInitializedChanged);
        QSignalSpy homeInitialized(home, &ScreenViewModel::isInitializedChanged);
        QSignalSpy shellActive(shell.get(), &ScreenViewModel::isActiveChanged);
        QSignalSpy homeActive(home, &ScreenViewModel::isActiveChanged);
        QStringList order;
        connect(home, &ScreenViewModel::isInitializedChanged, this, [&] { order << "home.initialize"; });
        connect(shell.get(), &ScreenViewModel::isInitializedChanged, this, [&] { order << "shell.initialize"; });
        connect(home, &ScreenViewModel::isActiveChanged, this, [&] { order << (home->isActive() ? "home.activate" : "home.deactivate"); });
        connect(shell.get(), &ScreenViewModel::isActiveChanged, this, [&] { order << (shell->isActive() ? "shell.activate" : "shell.deactivate"); });
        shell->initialize();
        shell->initialize();
        shell->activate();
        shell->activate();
        QCOMPARE(order, QStringList({"home.initialize", "shell.initialize", "home.activate", "shell.activate"}));
        shell->deactivate();
        shell->deactivate();
        shell->deactivate(true);
        shell->deactivate(true);
        QCOMPARE(order.mid(4), QStringList({"home.deactivate", "shell.deactivate"}));
        home->add(2);
        shell->activate();
        shell->deactivate(true);
        QCOMPARE(home->count(), 2);
        QCOMPARE(shellInitialized.count(), 1);
        QCOMPARE(homeInitialized.count(), 1);
        QCOMPARE(shellActive.count(), 4);
        QCOMPARE(homeActive.count(), 4);
        QVERIFY(home->isInitialized());
        QVERIFY(!home->isActive());
    }
};

QTEST_GUILESS_MAIN(CompositionTests)
#include "tst_composition.moc"
