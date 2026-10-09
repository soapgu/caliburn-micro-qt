#include <CaliburnMicroQt/WindowManager.h>
#include <CaliburnMicroQt/ConfirmActionViewModel.h>
#include <CaliburnMicroQt/ViewRegistry.h>
#include <ViewModelComposition.h>
#include <QQmlApplicationEngine>
#include <QQmlExtensionPlugin>
#include <QLibraryInfo>
#include <QQuickItem>
#include <QQuickWindow>
#include <QQuickStyle>
#include <QtTest>

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

static QQuickWindow *loadShell(QQmlApplicationEngine &engine, ShellViewModel &shell)
{
    engine.setImportPathList({"qrc:/qt/qml", QLibraryInfo::path(QLibraryInfo::QmlImportsPath)});
    engine.setInitialProperties({{"viewModel", QVariant::fromValue(&shell)}});
    engine.load(ViewRegistry::viewUrl(&shell));
    return engine.rootObjects().isEmpty() ? nullptr
        : qobject_cast<QQuickWindow *>(engine.rootObjects().front());
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
        QVERIFY(ViewRegistry::registerView<ConfirmActionViewModel>(QUrl("qrc:/qt/qml/Caliburn/Micro/Qt/ConfirmActionView.qml")));
        QVERIFY(ViewRegistry::registerView<CustomDialogVm>(QUrl("qrc:/tests/fixtures/CustomDialog.qml")));
        QVERIFY(ViewRegistry::registerView<MissingDialog>(QUrl("qrc:/tests/absent.qml")));
        QVERIFY(ViewRegistry::registerView<NonVisualDialog>(QUrl("qrc:/tests/fixtures/NonVisual.qml")));
        QVERIFY(ViewRegistry::registerView<SyntaxDialog>(QUrl("qrc:/tests/fixtures/SyntaxError.qml")));
        QVERIFY(ViewRegistry::registerView<BadInjectionDialog>(QUrl("qrc:/tests/fixtures/MissingProperty.qml")));
        QVERIFY(ViewRegistry::freeze());
    }
    void homeAcceptCancelAndModalInput() {
        auto shell = buildShell(); shell->activate(); shell->home()->add(3);
        QQmlApplicationEngine engine;
        QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        auto *window = loadShell(engine, *shell); QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));
        auto *input = window->findChild<QQuickItem *>("focusInput"); QVERIFY(input);
        input->forceActiveFocus();
        shell->home()->reset();
        QTRY_VERIFY(shell->windowManager()->busy());
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
        QVERIFY(shell->windowManager()->busy());
        cancel->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_Return);
        QTRY_VERIFY(!shell->home()->resetPending());
        QCOMPARE(shell->home()->count(), 3);
        QTRY_VERIFY(input->hasActiveFocus());
        shell->home()->reset();
        accept = window->findChild<QQuickItem *>("dialogAccept"); QVERIFY(accept);
        click(window, accept);
        QTRY_COMPARE(shell->home()->count(), 0);
        QVERIFY(!shell->windowManager()->busy());
        QCOMPARE(warnings.count(), 0);
        shell->deactivate(true); window->close();
    }
    void escapeAndPageDeactivation() {
        auto shell = buildShell(); shell->activate(); shell->home()->add(2);
        QQmlApplicationEngine engine;
        auto *window = loadShell(engine, *shell); QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));
        shell->home()->reset();
        QTRY_VERIFY(shell->windowManager()->busy());
        QTest::keyClick(window, Qt::Key_Escape);
        QTRY_VERIFY(!shell->windowManager()->busy());
        QCOMPARE(shell->home()->count(), 2);
        shell->home()->reset();
        QVERIFY(shell->showDetail());
        QTRY_VERIFY(!shell->windowManager()->busy());
        QVERIFY(!shell->home()->resetPending());
        QVERIFY(shell->detail()->goBack());
        QCOMPARE(shell->home()->count(), 2);
        window->close(); shell->deactivate(true);
    }
    void customVmAndViewBeforeVmDestruction() {
        auto shell = buildShell(); shell->activate();
        QQmlApplicationEngine engine;
        auto *window = loadShell(engine, *shell); QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));
        auto *manager = shell->windowManager();
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
        auto shell = buildShell(); shell->activate();
        QQmlApplicationEngine engine;
        auto *window = loadShell(engine, *shell); QVERIFY(window);
        std::unique_ptr<ScreenViewModel> vm;
        if (kind == 0) vm = std::make_unique<MissingDialog>();
        if (kind == 1) vm = std::make_unique<NonVisualDialog>();
        if (kind == 2) vm = std::make_unique<SyntaxDialog>();
        if (kind == 3) vm = std::make_unique<BadInjectionDialog>();
        auto future = shell->windowManager()->showDialogAsync(std::move(vm), shell.get());
        QTRY_VERIFY(future.isFinished());
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, future.result());
        QVERIFY(!shell->windowManager()->busy());
        window->close(); shell->deactivate(true);
    }
    void hostDestructionCompletesRequest() {
        auto shell = buildShell(); shell->activate();
        auto engine = std::make_unique<QQmlApplicationEngine>();
        QVERIFY(loadShell(*engine, *shell));
        auto *manager = shell->windowManager();
        auto future = manager->showDialogAsync(std::make_unique<CustomDialogVm>(*manager), shell.get());
        engine.reset();
        QTRY_VERIFY(future.isFinished());
        QCOMPARE(future.result(), DialogResult{});
        QVERIFY(!manager->busy()); shell->deactivate(true);
    }
};
QTEST_MAIN(DialogTests)
#include "tst_dialog.moc"
