#include <CaliburnMicroQt/ConfirmActionViewModel.h>
#include <CaliburnMicroQt/ViewRegistry.h>
#include <CaliburnMicroQt/WindowManager.h>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QThread>
#include <QtTest>
#include <memory>
#include <stdexcept>

Q_IMPORT_QML_PLUGIN(CaliburnMicroQtPlugin)

static std::unique_ptr<QQuickWindow> createWindow(QQmlEngine &engine)
{
    QQmlComponent component(&engine);
    component.setData(R"(
        import QtQuick
        import QtQuick.Controls
        ApplicationWindow {
            width: 480; height: 360
            Column { objectName: "businessLayout"; Item { width: 20; height: 30 } }
        }
    )", QUrl());
    std::unique_ptr<QQuickWindow> window(qobject_cast<QQuickWindow *>(component.create()));
    if (!window)
        qWarning().noquote() << component.errorString();
    return window;
}

static std::unique_ptr<ConfirmActionViewModel> confirmation(WindowManager &manager)
{
    return std::make_unique<ConfirmActionViewModel>(
        ConfirmationRequest{QStringLiteral("测试"), QStringLiteral("确认操作")}, manager);
}

class FrameworkDialogTests : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase() { QQuickStyle::setStyle(QStringLiteral("Basic")); }

    void registryMapping()
    {
        WindowManager manager;
        auto model = confirmation(manager);
        const QUrl defaultUrl(QStringLiteral("qrc:/qt/qml/Caliburn/Micro/Qt/ConfirmActionView.qml"));
        QCOMPARE(ViewRegistry::viewUrl(model.get()), defaultUrl); // 无 Bootstrapper、无显式登记。
        const QString mode = qEnvironmentVariable("CMQT_CONFIRM_MAPPING");
        const QUrl selected = mode.isEmpty() ? defaultUrl : QUrl(mode == "missing"
            ? QStringLiteral("qrc:/tests/absent-confirm.qml")
            : QStringLiteral("qrc:/tests/fixtures/OverrideConfirm.qml"));
        if (!mode.isEmpty()) {
            QVERIFY(ViewRegistry::registerView<ConfirmActionViewModel>(selected));
            QVERIFY(ViewRegistry::registerView<ConfirmActionViewModel>(selected));
            QTest::ignoreMessage(QtWarningMsg, "ViewRegistry：类型映射冲突 ConfirmActionViewModel");
            QVERIFY(!ViewRegistry::registerView<ConfirmActionViewModel>(defaultUrl));
        }
        QCOMPARE(ViewRegistry::viewUrl(model.get()), selected);
        QVERIFY(ViewRegistry::freeze());
        QTest::ignoreMessage(QtWarningMsg, "ViewRegistry：配置已冻结，拒绝登记");
        QVERIFY(!ViewRegistry::registerView<ConfirmActionViewModel>(defaultUrl));
        QQmlEngine engine;
        auto window = createWindow(engine);
        QVERIFY(window && manager.attachToWindow(window.get()));
        QObject requester;
        auto *vm = model.get();
        auto future = manager.showDialogAsync(std::move(model), &requester);
        if (mode == "missing") {
            QTRY_VERIFY(future.isFinished());
            QVERIFY_THROWS_EXCEPTION(std::runtime_error, future.result());
            QVERIFY(!manager.busy());
            return;
        }
        auto *host = window->findChild<QQuickItem *>(QStringLiteral("dialogHost"));
        QVERIFY(host);
        auto *view = host->property("dialogItem").value<QQuickItem *>();
        QVERIFY(view);
        if (!mode.isEmpty())
            QCOMPARE(view->objectName(), QStringLiteral("overrideConfirm"));
        vm->accept();
        QTRY_VERIFY(future.isFinished());
        QCOMPARE(future.result(), DialogResult(true));
        auto cancelModel = confirmation(manager);
        vm = cancelModel.get();
        auto canceled = manager.showDialogAsync(std::move(cancelModel), &requester);
        vm->cancel();
        QTRY_VERIFY(canceled.isFinished());
        QCOMPARE(canceled.result(), DialogResult(false));
    }

    void mountingValidationAndLayout()
    {
        QQmlEngine engine;
        auto first = createWindow(engine);
        auto second = createWindow(engine);
        QVERIFY(first && second);
        WindowManager manager;
        QQuickWindow nativeWindow;
        QTest::ignoreMessage(QtWarningMsg, "WindowManager：窗口没有可用的 QML 引擎");
        QVERIFY(!manager.attachToWindow(&nativeWindow));
        QTest::ignoreMessage(QtWarningMsg, "WindowManager：宿主窗口或线程无效");
        QVERIFY(!manager.attachToWindow(nullptr));
        QTest::ignoreMessage(QtWarningMsg, "WindowManager：后备焦点必须属于宿主窗口");
        QVERIFY(!manager.attachToWindow(first.get(), second->contentItem()));
        auto *layout = first->findChild<QQuickItem *>(QStringLiteral("businessLayout"));
        QVERIFY(layout);
        const QSizeF businessSize(layout->width(), layout->height());
        QVERIFY(manager.attachToWindow(first.get()));
        auto *host = first->findChild<QQuickItem *>(QStringLiteral("dialogHost"));
        QVERIFY(host && host->parentItem() == first->contentItem());
        QVERIFY(host->parentItem() != layout);
        QCOMPARE(QSizeF(layout->width(), layout->height()), businessSize);
        QCOMPARE(host->width(), first->contentItem()->width());
        first->setWidth(620);
        QTRY_COMPARE(host->width(), first->contentItem()->width());
        QVERIFY(manager.attachToWindow(first.get()));
        QCOMPARE(first->findChildren<QQuickItem *>(QStringLiteral("dialogHost")).size(), 1);
        QTest::ignoreMessage(QtWarningMsg, "WindowManager：已有宿主或尚未完成的请求，须先解除挂载");
        QVERIFY(!manager.attachToWindow(second.get()));
        WindowManager other;
        QTest::ignoreMessage(QtWarningMsg, "WindowManager：目标窗口已有弹窗宿主");
        QVERIFY(!other.attachToWindow(first.get()));
        QTest::ignoreMessage(QtWarningMsg, "WindowManager：宿主窗口或线程无效");
        auto worker = std::unique_ptr<QThread>(QThread::create([&] {
            QVERIFY(!manager.attachToWindow(first.get()));
        }));
        worker->start();
        QVERIFY(worker->wait(5000));
        manager.detachFromWindow();
        QVERIFY(!first->findChild<QQuickItem *>(QStringLiteral("dialogHost")));
        QVERIFY(manager.attachToWindow(second.get()));
        QVERIFY(other.attachToWindow(first.get())); // 独立窗口具有独立服务。
    }

    void existingManualHostIsNotReplaced()
    {
        WindowManager manager;
        QQmlEngine engine;
        auto window = createWindow(engine);
        QVERIFY(window);
        QQmlComponent component(&engine);
        component.setData("import Caliburn.Micro.Qt 1.0; DialogHostState {}", QUrl());
        std::unique_ptr<QObject> state(component.create());
        QVERIFY(state);
        QVERIFY(state->setProperty("manager", QVariant::fromValue(static_cast<IWindowManager *>(&manager))));
        QTest::ignoreMessage(QtWarningMsg, "WindowManager：已有宿主或尚未完成的请求，须先解除挂载");
        QVERIFY(!manager.attachToWindow(window.get()));
        state.reset();
        QVERIFY(manager.attachToWindow(window.get()));
    }

    void destructionCompletesRequest_data()
    {
        QTest::addColumn<QString>("kind");
        for (const auto *kind : {"detach", "window", "engine", "manager", "requester"})
            QTest::newRow(kind) << QString::fromLatin1(kind);
    }

    void destructionCompletesRequest()
    {
        QFETCH(QString, kind);
        auto manager = std::make_unique<WindowManager>();
        auto engine = std::make_unique<QQmlEngine>();
        auto window = createWindow(*engine);
        auto requester = std::make_unique<QObject>();
        QVERIFY(window && manager->attachToWindow(window.get()));
        auto model = confirmation(*manager);
        QPointer<ConfirmActionViewModel> weak = model.get();
        auto future = manager->showDialogAsync(std::move(model), requester.get());
        auto *host = window->findChild<QQuickItem *>(QStringLiteral("dialogHost"));
        QVERIFY(host);
        QPointer<QQuickItem> view = host->property("dialogItem").value<QQuickItem *>();
        QVERIFY(view);
        QStringList order;
        connect(view, &QObject::destroyed, this, [&] { order << "view"; });
        connect(weak, &QObject::destroyed, this, [&] { order << "vm"; });
        if (kind == "detach") manager->detachFromWindow();
        if (kind == "window") window.reset();
        if (kind == "engine") engine.reset(); // 窗口由 C++ 持有，引擎单独销毁。
        if (kind == "manager") manager.reset();
        if (kind == "requester") requester.reset();
        QTRY_VERIFY(future.isFinished());
        QCOMPARE(future.result(), DialogResult{});
        if (manager) QVERIFY(!manager->busy());
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(!view && !weak);
        QCOMPARE(order, QStringList({"view", "vm"}));
    }
};

QTEST_MAIN(FrameworkDialogTests)
#include "tst_framework_dialog.moc"
