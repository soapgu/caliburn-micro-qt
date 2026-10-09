#include <CaliburnMicroQt/WindowManager.h>
#include <CaliburnMicroQt/ConfirmActionViewModel.h>
#include <CaliburnMicroQt/ViewRegistry.h>
#include <ViewModelComposition.h>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlExtensionPlugin>
#include <QLibraryInfo>
#include <QQuickItem>
#include <QQuickWindow>
#include <QQuickStyle>
#include <QtTest>
#include <memory>
#include <stdexcept>

Q_IMPORT_QML_PLUGIN(CaliburnMicroQtPlugin)
Q_IMPORT_QML_PLUGIN(CaliburnExampleModulePlugin)

class CustomDialogVm : public ScreenViewModel
{
    Q_OBJECT
public:
    explicit CustomDialogVm(IWindowManager &manager) : m_manager(manager) {}
    Q_INVOKABLE void finish() { m_manager.closeDialog(this, true); }
private:
    IWindowManager &m_manager;
};
class MissingDialog : public ScreenViewModel { Q_OBJECT };
class NonVisualDialog : public ScreenViewModel { Q_OBJECT };
class SyntaxDialog : public ScreenViewModel { Q_OBJECT };
class BadInjectionDialog : public ScreenViewModel { Q_OBJECT };

// 用抽象服务的另一种实现验证 manager 的公开类型与具体服务限制。
class UnsupportedWindowManager : public IWindowManager
{
public:
    bool busy() const override { return false; }
    ScreenViewModel *currentDialog() const override { return nullptr; }
    QFuture<DialogResult> showDialogAsync(std::unique_ptr<ScreenViewModel>, QObject *) override
    { return {}; }
    void closeDialog(ScreenViewModel *, DialogResult) override {}
    void cancelDialogsFor(QObject *) override {}
};

// 不包含 State 的 C++ 头文件；通过公共模块创建并调用实际 QML 接口。
static std::unique_ptr<QObject> createDialogState(QQmlEngine &engine)
{
    engine.setImportPathList({"qrc:/qt/qml", QLibraryInfo::path(QLibraryInfo::QmlImportsPath)});
    QQmlComponent component(&engine);
    component.setData(R"(
        import Caliburn.Micro.Qt 1.0
        DialogHostState {
            property string hiddenRequest: ""
            onHideRequested: (id) => { hiddenRequest = id }
            function attachService(service) { manager = service }
            function setHostAvailable(value) { available = value }
            function failRequest(id, message) { failed(id, message) }
            function releaseRequest(id) { released(id) }
            function dismissRequest(id) { dismiss(id) }
        }
    )", QUrl());
    std::unique_ptr<QObject> state(component.create());
    if (!state)
        qWarning().noquote() << component.errorString();
    return state;
}

static bool attachService(QObject *state, IWindowManager *manager)
{
    return QMetaObject::invokeMethod(state, "attachService",
        Q_ARG(QVariant, QVariant::fromValue(manager)));
}

static bool setHostAvailable(QObject *state, bool available)
{
    return QMetaObject::invokeMethod(state, "setHostAvailable", Q_ARG(QVariant, available));
}

static bool reportRequest(QObject *state, const char *method, const QString &id)
{
    return QMetaObject::invokeMethod(state, method, Q_ARG(QVariant, id));
}

static bool reportFailure(QObject *state, const QString &id)
{
    return QMetaObject::invokeMethod(state, "failRequest", Q_ARG(QVariant, id),
        Q_ARG(QVariant, QStringLiteral("自定义宿主展示失败")));
}

static QQuickWindow *loadShell(QQmlApplicationEngine &engine, ShellViewModel &shell, WindowManager &windows)
{
    engine.setImportPathList({"qrc:/qt/qml", QLibraryInfo::path(QLibraryInfo::QmlImportsPath)});
    engine.setInitialProperties({{"viewModel", QVariant::fromValue(&shell)}});
    engine.load(ViewRegistry::viewUrl(&shell));
    auto *window = engine.rootObjects().isEmpty() ? nullptr
        : qobject_cast<QQuickWindow *>(engine.rootObjects().front());
    return window && windows.attachToWindow(window) ? window : nullptr;
}
static void click(QQuickWindow *window, QQuickItem *item)
{
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
        item->mapToScene(QPointF(item->width()/2, item->height()/2)).toPoint());
}

class DialogTests : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase() {
        QQuickStyle::setStyle("Basic");
        qmlRegisterUncreatableType<CustomDialogVm>("DialogTest", 1, 0, "CustomDialogVm", "测试 VM");
        QVERIFY(ViewRegistry::registerView<ShellViewModel>(QUrl("qrc:/qt/qml/CaliburnExample/views/ShellView.qml")));
        QVERIFY(ViewRegistry::registerView<HomeViewModel>(QUrl("qrc:/qt/qml/CaliburnExample/views/HomeView.qml")));
        QVERIFY(ViewRegistry::registerView<DetailViewModel>(QUrl("qrc:/qt/qml/CaliburnExample/views/DetailView.qml")));
        QVERIFY(ViewRegistry::registerView<CustomDialogVm>(QUrl("qrc:/tests/fixtures/CustomDialog.qml")));
        QVERIFY(ViewRegistry::registerView<MissingDialog>(QUrl("qrc:/tests/absent.qml")));
        QVERIFY(ViewRegistry::registerView<NonVisualDialog>(QUrl("qrc:/tests/fixtures/NonVisual.qml")));
        QVERIFY(ViewRegistry::registerView<SyntaxDialog>(QUrl("qrc:/tests/fixtures/SyntaxError.qml")));
        QVERIFY(ViewRegistry::registerView<BadInjectionDialog>(QUrl("qrc:/tests/fixtures/MissingProperty.qml")));
        QVERIFY(ViewRegistry::freeze());
    }

    void publicStateServiceAssociation() {
        auto manager = std::make_unique<WindowManager>();
        UnsupportedWindowManager unsupported;
        QQmlEngine engine;
        auto first = createDialogState(engine);
        auto second = createDialogState(engine);
        QVERIFY(first && second);
        QSignalSpy managerChanged(first.get(), SIGNAL(managerChanged()));
        QSignalSpy availableChanged(first.get(), SIGNAL(availableChanged()));
        QSignalSpy modelChanged(first.get(), SIGNAL(modelChanged()));
        QVERIFY(attachService(first.get(), manager.get()));
        QCOMPARE(first->property("manager").value<QObject *>(), manager.get());
        QVERIFY(setHostAvailable(first.get(), true));
        QVERIFY(first->property("available").toBool());
        QVERIFY(attachService(first.get(), manager.get()));
        QVERIFY(setHostAvailable(first.get(), true));
        QCOMPARE(managerChanged.count(), 1);
        QCOMPARE(modelChanged.count(), 1);
        QCOMPARE(availableChanged.count(), 1);

        QTest::ignoreMessage(QtWarningMsg, "DialogHost：窗口服务不受支持或已有宿主");
        QVERIFY(attachService(second.get(), manager.get()));
        QVERIFY(!second->property("manager").value<QObject *>());
        QCOMPARE(first->property("manager").value<QObject *>(), manager.get());
        QTest::ignoreMessage(QtWarningMsg, "DialogHost：窗口服务不受支持或已有宿主");
        QVERIFY(attachService(second.get(), &unsupported));
        QVERIFY(!second->property("manager").value<QObject *>());
        QVERIFY(reportFailure(second.get(), "unused"));
        QVERIFY(reportRequest(second.get(), "dismissRequest", "unused"));
        QVERIFY(reportRequest(second.get(), "releaseRequest", "unused"));

        QVERIFY(attachService(first.get(), nullptr));
        QVERIFY(attachService(second.get(), manager.get()));
        QCOMPARE(second->property("manager").value<QObject *>(), manager.get());
        QSignalSpy detached(second.get(), SIGNAL(managerChanged()));
        manager.reset();
        QVERIFY(!second->property("manager").value<QObject *>());
        QVERIFY(!second->property("model").value<QObject *>());
        QCOMPARE(detached.count(), 1);
    }

    void publicStateRequestProtocol_data() {
        QTest::addColumn<QByteArray>("closeMethod");
        QTest::newRow("dismiss") << QByteArray("dismissRequest");
        QTest::newRow("failed") << QByteArray("failRequest");
        QTest::newRow("unavailable") << QByteArray("setHostAvailable");
    }

    void publicStateRequestProtocol() {
        QFETCH(QByteArray, closeMethod);
        QStringList destructionOrder;
        WindowManager manager;
        QObject requester;
        QQmlEngine engine;
        auto state = createDialogState(engine);
        QVERIFY(state);
        QVERIFY(attachService(state.get(), &manager));
        QVERIFY(setHostAvailable(state.get(), true));
        auto vm = std::make_unique<CustomDialogVm>(manager);
        QPointer<CustomDialogVm> weak = vm.get();
        auto future = manager.showDialogAsync(std::move(vm), &requester);
        QVERIFY(!future.isFinished());
        QVERIFY(weak->isInitialized() && weak->isActive());
        QCOMPARE(state->property("model").value<QObject *>(), weak.data());
        const auto id = state->property("requestId").toString();
        QVERIFY(!id.isEmpty());

        QQmlComponent viewComponent(&engine);
        viewComponent.setData("import QtQml; import Caliburn.Micro.Qt 1.0; "
            "QtObject { required property ScreenViewModel viewModel }", QUrl());
        std::unique_ptr<QObject> view(viewComponent.createWithInitialProperties(
            {{"viewModel", QVariant::fromValue(weak.data())}}));
        QVERIFY2(view, qPrintable(viewComponent.errorString()));
        connect(view.get(), &QObject::destroyed, this, [&] { destructionOrder << "view"; });
        connect(weak.data(), &QObject::destroyed, this, [&] { destructionOrder << "vm"; });
        // 提前报告 released 不能自行关闭活动请求。
        QVERIFY(reportRequest(state.get(), "releaseRequest", id));
        QVERIFY(manager.busy() && !future.isFinished());
        if (closeMethod == "failRequest")
            QVERIFY(reportFailure(state.get(), id));
        else if (closeMethod == "setHostAvailable")
            QVERIFY(setHostAvailable(state.get(), false));
        else
            QVERIFY(reportRequest(state.get(), closeMethod.constData(), id));

        QCOMPARE(state->property("hiddenRequest").toString(), id);
        QVERIFY(!state->property("model").value<QObject *>());
        QCOMPARE(state->property("requestId").toString(), id);
        QVERIFY(manager.busy() && !future.isFinished());
        QVERIFY(weak->isActive());
        // 关闭中的重复报告不能改写已确定的结果。
        QVERIFY(reportFailure(state.get(), id));
        QVERIFY(reportRequest(state.get(), "dismissRequest", id));
        view.reset();
        QVERIFY(reportRequest(state.get(), "releaseRequest", id));
        QVERIFY(future.isFinished());
        QVERIFY(!manager.busy());
        QCOMPARE(state->property("requestId").toString(), QString());
        QVERIFY(weak && weak->isInitialized() && !weak->isActive());
        if (closeMethod == "failRequest")
            QVERIFY_THROWS_EXCEPTION(std::runtime_error, future.result());
        else
            QCOMPARE(future.result(), DialogResult{});
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(!weak);
        QCOMPARE(destructionOrder, QStringList({"view", "vm"}));

        QVERIFY(setHostAvailable(state.get(), true));
        auto nextVm = std::make_unique<CustomDialogVm>(manager);
        QPointer<CustomDialogVm> next = nextVm.get();
        auto nextFuture = manager.showDialogAsync(std::move(nextVm), &requester);
        const auto nextId = state->property("requestId").toString();
        QVERIFY(!nextId.isEmpty() && nextId != id);
        QVERIFY(reportFailure(state.get(), id));
        QVERIFY(reportRequest(state.get(), "dismissRequest", id));
        QVERIFY(reportRequest(state.get(), "releaseRequest", id));
        QVERIFY(manager.busy() && !nextFuture.isFinished());
        QCOMPARE(state->property("model").value<QObject *>(), next.data());
        manager.closeDialog(next.data(), true);
        QVERIFY(reportRequest(state.get(), "releaseRequest", nextId));
        QVERIFY(nextFuture.isFinished());
        QCOMPARE(nextFuture.result(), DialogResult(true));
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(!next);
    }

    void homeAcceptCancelAndModalInput() {
        auto windows = std::make_shared<WindowManager>();
        auto shell = buildShell(windows); shell->activate(); shell->home()->add(3);
        QQmlApplicationEngine engine;
        QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        auto *window = loadShell(engine, *shell, *windows); QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));
        auto *input = window->findChild<QQuickItem *>("focusInput"); QVERIFY(input);
        input->forceActiveFocus();
        shell->home()->reset();
        QTRY_VERIFY(windows->busy());
        auto *cancel = window->findChild<QQuickItem *>("dialogCancel");
        auto *accept = window->findChild<QQuickItem *>("dialogAccept");
        QVERIFY(cancel && accept);
        QTRY_VERIFY(cancel->hasActiveFocus());
        QTest::keyClick(window, Qt::Key_Tab); QTRY_VERIFY(accept->hasActiveFocus());
        QTest::keyClick(window, Qt::Key_Tab); QTRY_VERIFY(cancel->hasActiveFocus());
        QTest::keyClick(window, Qt::Key_Backtab); QTRY_VERIFY(accept->hasActiveFocus());
        QTest::keyClick(window, Qt::Key_2); QCOMPARE(shell->home()->count(), 3);
        auto *navigation = window->findChild<QQuickItem *>("showDetail"); QVERIFY(navigation);
        click(window, navigation); QVERIFY(!shell->detail());
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, QPoint(3,3));
        QVERIFY(windows->busy());
        cancel->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_Return);
        QTRY_VERIFY(!shell->home()->resetPending());
        QCOMPARE(shell->home()->count(), 3);
        QTRY_VERIFY(input->hasActiveFocus());
        shell->home()->reset();
        accept = window->findChild<QQuickItem *>("dialogAccept"); QVERIFY(accept);
        click(window, accept);
        QTRY_COMPARE(shell->home()->count(), 0);
        QVERIFY(!windows->busy());
        QCOMPARE(warnings.count(), 0);
        shell->deactivate(true); window->close();
    }
    void escapeAndPageDeactivation() {
        auto windows = std::make_shared<WindowManager>();
        auto shell = buildShell(windows); shell->activate(); shell->home()->add(2);
        QQmlApplicationEngine engine;
        auto *window = loadShell(engine, *shell, *windows); QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));
        shell->home()->reset();
        QTRY_VERIFY(windows->busy());
        QTest::keyClick(window, Qt::Key_Escape);
        QTRY_VERIFY(!windows->busy());
        QCOMPARE(shell->home()->count(), 2);
        shell->home()->reset();
        QVERIFY(shell->showDetail());
        QTRY_VERIFY(!windows->busy());
        QVERIFY(!shell->home()->resetPending());
        QVERIFY(shell->detail()->goBack());
        QCOMPARE(shell->home()->count(), 2);
        window->close(); shell->deactivate(true);
    }
    void customVmAndViewBeforeVmDestruction() {
        auto windows = std::make_shared<WindowManager>();
        auto shell = buildShell(windows); shell->activate();
        QQmlApplicationEngine engine;
        auto *window = loadShell(engine, *shell, *windows); QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));
        auto *manager = windows.get();
        auto vm = std::make_unique<CustomDialogVm>(*manager);
        QPointer<CustomDialogVm> weak = vm.get();
        auto future = manager->showDialogAsync(std::move(vm), shell.get());
        auto *button = window->findChild<QQuickItem *>("customDialogAccept"); QVERIFY(button);
        auto *host = window->findChild<QQuickItem *>("dialogHost"); QVERIFY(host);
        QPointer<QQuickItem> view = host->property("dialogItem").value<QQuickItem *>(); QVERIFY(view);
        QStringList order;
        connect(view, &QObject::destroyed, this, [&] { order << "view"; });
        connect(weak, &QObject::destroyed, this, [&] { order << "vm"; });
        click(window, button);
        QTRY_VERIFY(future.isFinished()); QCOMPARE(future.result(), DialogResult(true));
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(!weak && !view);
        QCOMPARE(order, QStringList({"view", "vm"}));
        window->close(); shell->deactivate(true);
    }
    void loadFailures_data() {
        QTest::addColumn<int>("kind");
        QTest::newRow("缺失资源") << 0;
        QTest::newRow("非 Item") << 1;
        QTest::newRow("语法错误") << 2;
        QTest::newRow("注入失败") << 3;
    }
    void loadFailures() {
        QFETCH(int, kind);
        auto windows = std::make_shared<WindowManager>();
        auto shell = buildShell(windows); shell->activate();
        QQmlApplicationEngine engine;
        auto *window = loadShell(engine, *shell, *windows); QVERIFY(window);
        std::unique_ptr<ScreenViewModel> vm;
        if (kind == 0) vm = std::make_unique<MissingDialog>();
        if (kind == 1) vm = std::make_unique<NonVisualDialog>();
        if (kind == 2) vm = std::make_unique<SyntaxDialog>();
        if (kind == 3) vm = std::make_unique<BadInjectionDialog>();
        auto future = windows->showDialogAsync(std::move(vm), shell.get());
        QTRY_VERIFY(future.isFinished());
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, future.result());
        QVERIFY(!windows->busy());
        window->close(); shell->deactivate(true);
    }
    void hostDestructionCompletesRequest() {
        auto windows = std::make_shared<WindowManager>();
        auto shell = buildShell(windows); shell->activate();
        auto engine = std::make_unique<QQmlApplicationEngine>();
        QVERIFY(loadShell(*engine, *shell, *windows));
        auto *manager = windows.get();
        auto future = manager->showDialogAsync(std::make_unique<CustomDialogVm>(*manager), shell.get());
        engine.reset();
        QTRY_VERIFY(future.isFinished());
        QCOMPARE(future.result(), DialogResult{});
        QVERIFY(!manager->busy()); shell->deactivate(true);
    }
};
QTEST_MAIN(DialogTests)
#include "tst_dialog.moc"
