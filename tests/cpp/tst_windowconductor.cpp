#include "WindowConductor.h"
#include <CaliburnMicroQt/ScreenViewModel.h>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QtTest>
#include <memory>
#include <stdexcept>

class GuardedRoot : public ScreenViewModel
{
    Q_OBJECT
public:
    bool delayed = false;
    bool allowed = true;
    bool throws = false;
    int checks = 0;
    int closes = 0;
    CloseCallback pending;
    void canClose(CloseCallback callback) override
    {
        ++checks;
        if (throws) throw std::runtime_error("测试根守卫异常");
        if (delayed) pending = std::move(callback);
        else callback(allowed);
    }
protected:
    void onDeactivate(bool close) override { if (close) ++closes; }
};

class WindowConductorTests : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase() { QGuiApplication::setQuitOnLastWindowClosed(false); }

    void permission_data()
    {
        QTest::addColumn<bool>("delayed");
        QTest::addColumn<bool>("allowed");
        QTest::newRow("立即同意") << false << true;
        QTest::newRow("立即拒绝") << false << false;
        QTest::newRow("延后同意") << true << true;
        QTest::newRow("延后拒绝") << true << false;
    }
    void permission()
    {
        QFETCH(bool, delayed);
        QFETCH(bool, allowed);
        QQuickWindow window;
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        GuardedRoot root;
        root.delayed = delayed;
        root.allowed = allowed;
        root.activate();
        WindowConductor bridge(&window, &root);
        connect(&bridge, &WindowConductor::windowClosed, &root, [&] { root.deactivate(true); });
        QSignalSpy closed(&bridge, &WindowConductor::windowClosed);
        QSignalSpy completed(&root, &ScreenViewModel::deactivated);
        QVERIFY(!window.close());
        QCOMPARE(root.checks, 0); // 不在原关闭调用栈内运行守卫。
        QTRY_COMPARE(root.checks, 1);
        if (delayed) {
            QVERIFY(window.isVisible() && root.isActive());
            QVERIFY(!window.close());
            QCoreApplication::processEvents();
            QCOMPARE(root.checks, 1);
            auto answer = std::move(root.pending);
            answer(allowed);
        }
        QTRY_COMPARE(window.isVisible(), !allowed);
        QCOMPARE(root.closes, allowed ? 1 : 0);
        QCOMPARE(completed.count(), allowed ? 1 : 0);
        QCOMPARE(closed.count(), allowed ? 1 : 0);
        QCOMPARE(root.checks, 1);
        QCoreApplication::processEvents();
        QCOMPARE(closed.count(), allowed ? 1 : 0);
    }

    void rootTryCloseAndDefaultPermission()
    {
        QQuickWindow window;
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        ScreenViewModel root;
        root.activate();
        WindowConductor bridge(&window, &root);
        connect(&bridge, &WindowConductor::windowClosed, &root, [&] { root.deactivate(true); });
        QSignalSpy requested(&root, &ScreenViewModel::closeRequested);
        QSignalSpy completed(&root, &ScreenViewModel::deactivated);
        root.tryClose();
        QCOMPARE(requested.count(), 1);
        QTRY_VERIFY(!window.isVisible());
        QCOMPARE(completed.count(), 1);
    }

    void modelCloseAndOrdinaryDeactivation()
    {
        QQuickWindow window;
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        GuardedRoot root;
        root.activate();
        WindowConductor bridge(&window, &root);
        QSignalSpy closed(&bridge, &WindowConductor::windowClosed);
        root.deactivate(false);
        QCoreApplication::processEvents();
        QVERIFY(window.isVisible());
        QCOMPARE(root.checks, 0);
        root.activate();
        root.deactivate(true);
        QTRY_VERIFY(!window.isVisible());
        QCOMPARE(root.checks, 0);
        QCOMPARE(root.closes, 1);
        QCOMPARE(closed.count(), 1);
    }

    void qmlVetoPreservesModel()
    {
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.setData("import QtQuick; Window { visible: true; property bool veto: true; "
                          "onClosing: event => { if (veto) event.accepted = false } }", QUrl());
        std::unique_ptr<QObject> object(component.create());
        auto *window = qobject_cast<QQuickWindow *>(object.get());
        QVERIFY2(window, qPrintable(component.errorString()));
        QVERIFY(QTest::qWaitForWindowExposed(window));
        GuardedRoot root;
        root.activate();
        WindowConductor bridge(window, &root);
        connect(&bridge, &WindowConductor::windowClosed, &root, [&] { root.deactivate(true); });
        QSignalSpy closed(&bridge, &WindowConductor::windowClosed);
        QVERIFY(!window->close());
        QTRY_COMPARE(root.checks, 1);
        QVERIFY(window->isVisible() && root.isActive());
        QCOMPARE(closed.count(), 0);
        window->setProperty("veto", false);
        QVERIFY(!window->close());
        QTRY_VERIFY(!window->isVisible());
        QCOMPARE(root.checks, 2);
        QCOMPARE(root.closes, 1);
    }

    void destroyedParticipants_data()
    {
        QTest::addColumn<int>("participant");
        QTest::newRow("桥接销毁") << 0;
        QTest::newRow("窗口销毁") << 1;
        QTest::newRow("根对象销毁") << 2;
        QTest::newRow("解除桥接") << 3;
    }
    void destroyedParticipants()
    {
        QFETCH(int, participant);
        auto window = std::make_unique<QQuickWindow>();
        window->show();
        QVERIFY(QTest::qWaitForWindowExposed(window.get()));
        auto root = std::make_unique<GuardedRoot>();
        root->delayed = true;
        root->activate();
        auto bridge = std::make_unique<WindowConductor>(window.get(), root.get());
        QVERIFY(!window->close());
        QTRY_VERIFY(bool(root->pending));
        auto answer = std::move(root->pending);
        if (participant == 0) bridge.reset();
        if (participant == 1) window.reset();
        if (participant == 2) root.reset();
        if (participant == 3) bridge->detach();
        answer(true);
        QCoreApplication::processEvents();
        if (window) QVERIFY(window->isVisible());
        if (root) QVERIFY(root->isActive());
    }

    void guardExceptionAllowsRetry()
    {
        QQuickWindow window;
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        GuardedRoot root;
        root.throws = true;
        root.activate();
        WindowConductor bridge(&window, &root);
        QTest::ignoreMessage(QtWarningMsg, "WindowConductor：关闭守卫失败： 测试根守卫异常");
        QVERIFY(!window.close());
        QTRY_COMPARE(root.checks, 1);
        QVERIFY(window.isVisible() && root.isActive());
        root.throws = false;
        QVERIFY(!window.close());
        QTRY_VERIFY(!window.isVisible());
        QCOMPARE(root.checks, 2);
    }
};

QTEST_MAIN(WindowConductorTests)
#include "tst_windowconductor.moc"
