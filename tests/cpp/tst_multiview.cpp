#include <ViewModels.h>
#include <CaliburnMicroQt/BootstrapperBase.h>
#include <CaliburnMicroQt/Conductor.h>
#include <CaliburnMicroQt/ViewLocator.h>
#include <CaliburnMicroQt/WindowManager.h>
#include "../support/DialogWindowSupport.h"
#include <QQmlComponent>
#include <QQmlExtensionPlugin>
#include <QQuickStyle>
#include <QTimer>
#include <QtTest>

Q_IMPORT_QML_PLUGIN(CaliburnMicroQtPlugin)
Q_IMPORT_QML_PLUGIN(CaliburnMultiViewModulePlugin)

class WindowViewModel : public ScreenViewModel
{
    Q_OBJECT
public:
    bool allowClose = true;
    void canClose(CloseCallback callback) override { callback(allowClose); }
};
class DialogViewModel : public ScreenViewModel { Q_OBJECT };

static QQuickItem *hostItem(QObject *host)
{
    return host ? host->property("item").value<QQuickItem *>() : nullptr;
}

static std::unique_ptr<QObject> createHost(QQmlEngine &engine, QObject *model, const QVariant &context)
{
    QQmlComponent component(&engine, QUrl("qrc:/qt/qml/Caliburn/Micro/Qt/ViewHost.qml"));
    return std::unique_ptr<QObject>(component.createWithInitialProperties(
        {{QStringLiteral("model"), QVariant::fromValue(model)}, {QStringLiteral("context"), context}}));
}

// 在独立进程执行，避免 Bootstrapper 冻结进程级定位配置影响其他场景。
class ContextBootstrapper final : public BootstrapperBase
{
public:
    using BootstrapperBase::BootstrapperBase;
    bool contextLoaded = false;
    bool closed = false;
protected:
    bool configure() override
    {
        return ViewLocator::addSearchRoot({QUrl("qrc:/multiview/")})
            && registerRootFactory<WindowViewModel>([this](std::shared_ptr<IWindowManager> windows) {
                m_windows = std::move(windows);
                auto model = std::make_unique<WindowViewModel>();
                m_model = model.get();
                return model;
            });
    }
    void onStartup() override
    {
        RootViewOptions options;
        options.window.geometry = QRect(30, 40, 420, 260);
        if (!displayRootViewFor<WindowViewModel>(options, QStringLiteral("Master"))) return;
        auto *window = m_windows->windowFor(m_model);
        contextLoaded = window && window->property("selectedContext") == QStringLiteral("Master")
            && window->size() == QSize(420, 260);
        connect(m_windows.get(), &IWindowManager::windowClosed, this, [this] { closed = true; });
        QTimer::singleShot(0, m_model, [this] { if (m_model) m_model->tryClose(); });
    }
private:
    std::shared_ptr<IWindowManager> m_windows;
    QPointer<WindowViewModel> m_model;
};

class MultiViewTests : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QQuickStyle::setStyle(QStringLiteral("Basic"));
        QVERIFY(ViewLocator::addSearchRoot({QUrl("qrc:/qt/qml/CaliburnMultiView/"), QStringLiteral("MultiView")}));
        QVERIFY(ViewLocator::addSearchRoot({QUrl("qrc:/multiview/")}));
        // 坏页面显式映射用于验证已选地址失败时不切换候选。
        for (const auto &context : {QStringLiteral("Broken"), QStringLiteral("Wrong"), QStringLiteral("Mismatch")})
            QVERIFY(ViewLocator::registerView<MultiView::HomeViewModel>(
                QUrl(QStringLiteral("qrc:/multiview/views/Home/%1.qml").arg(context)), context));
    }

    void qmlContextValues_data()
    {
        QTest::addColumn<QByteArray>("argument");
        QTest::addColumn<QString>("tail");
        QTest::newRow("omitted") << QByteArray() << QString("HomeView.qml");
        QTest::newRow("null") << QByteArray(", null") << QString("HomeView.qml");
        QTest::newRow("undefined") << QByteArray(", undefined") << QString("HomeView.qml");
        QTest::newRow("master") << QByteArray(", 'Master'") << QString("Home/Master.qml");
        QTest::newRow("caseSensitiveQrc") << QByteArray(", 'master'") << QString();
        QTest::newRow("empty") << QByteArray(", ''") << QString();
        QTest::newRow("number") << QByteArray(", 1") << QString();
        QTest::newRow("boolean") << QByteArray(", false") << QString();
        QTest::newRow("object") << QByteArray(", ({name: 'Master'})") << QString();
        QTest::newRow("array") << QByteArray(", ['Master']") << QString();
        QTest::newRow("boxedString") << QByteArray(", new String('Master')") << QString();
    }

    void qmlContextValues()
    {
        QFETCH(QByteArray, argument);
        QFETCH(QString, tail);
        QQmlEngine engine;
        MultiView::HomeViewModel model;
        QQmlComponent component(&engine);
        component.setData("import QtQml; import Caliburn.Micro.Qt 1.0; QtObject { required property QtObject model; "
                          "property url result: ViewLocator.resolve(model" + argument + ") }", QUrl());
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        std::unique_ptr<QObject> object(component.createWithInitialProperties({{"model", QVariant::fromValue(&model)}}));
        QVERIFY2(object, qPrintable(component.errorString()));
        const auto url = object->property("result").toUrl();
        if (tail.isEmpty()) QVERIFY(url.isEmpty());
        else QCOMPARE(url, QUrl("qrc:/qt/qml/CaliburnMultiView/views/" + tail));
    }

    void twoHostsAndContextChanges()
    {
        QQmlEngine engine;
        MultiView::HomeViewModel model;
        QSignalSpy lifecycle(&model, &ScreenViewModel::isActiveChanged);
        auto master = createHost(engine, &model, QStringLiteral("Master"));
        auto detail = createHost(engine, &model, QStringLiteral("Detail"));
        QVERIFY(master && detail);
        auto *first = hostItem(master.get());
        QPointer<QQuickItem> old = hostItem(detail.get());
        QVERIFY(first && old && first != old);
        QCOMPARE(first->objectName(), QString("masterView"));
        QCOMPARE(old->objectName(), QString("detailView"));
        QCOMPARE(first->property("viewModel").value<QObject *>(), &model);
        QCOMPARE(old->property("viewModel").value<QObject *>(), &model);
        auto *note = old->findChild<QObject *>("localNote");
        QVERIFY(note);
        note->setProperty("text", QStringLiteral("暂存输入"));
        model.setName(QStringLiteral("共享更新"));
        model.increment();
        QCOMPARE(old->findChild<QObject *>("nameSummary")->property("text"), QStringLiteral("共享更新"));
        QVERIFY(old->findChild<QObject *>("countSummary")->property("text").toString().endsWith('1'));
        detail->setProperty("context", QStringLiteral("Detail"));
        QCOMPARE(hostItem(detail.get()), old.data());
        detail->setProperty("context", QStringLiteral("Master"));
        QTRY_VERIFY(!old);
        QCOMPARE(hostItem(detail.get())->objectName(), QString("masterView"));
        QCOMPARE(hostItem(master.get()), first);
        detail->setProperty("context", QStringLiteral("Detail"));
        QCOMPARE(hostItem(detail.get())->findChild<QObject *>("localNote")->property("text").toString(), QString());
        QCOMPARE(model.name(), QStringLiteral("共享更新"));
        for (int i = 0; i < 10; ++i) {
            detail->setProperty("context", QStringLiteral("Master"));
            detail->setProperty("context", QStringLiteral("Detail"));
        }
        detail->setProperty("context", QVariant());
        QCOMPARE(hostItem(detail.get())->objectName(), QString("defaultView"));
        QCOMPARE(lifecycle.count(), 0);
        QVERIFY(!model.isInitialized());
    }

    void hostFailures_data()
    {
        QTest::addColumn<QVariant>("context");
        for (const char *name : {"Missing", "Broken", "Wrong", "Mismatch", "", "a..b"})
            QTest::newRow(*name ? name : "empty") << QVariant(QString::fromLatin1(name));
        QTest::newRow("number") << QVariant(1);
    }

    void hostFailures()
    {
        QFETCH(QVariant, context);
        QQmlEngine engine;
        MultiView::HomeViewModel model;
        auto host = createHost(engine, &model, QStringLiteral("Master"));
        QVERIFY(host && hostItem(host.get()));
        QPointer<QQuickItem> old = hostItem(host.get());
        host->setProperty("context", context);
        QVERIFY(!hostItem(host.get()));
        QVERIFY(!host->property("errorString").toString().isEmpty());
        QTRY_VERIFY(!old);
        host->setProperty("context", QStringLiteral("Detail"));
        QVERIFY(hostItem(host.get()));
        QCOMPARE(host->property("errorString").toString(), QString());
    }

    void managedModelAndMultipleViews()
    {
        QQmlEngine engine;
        Conductor<MultiView::HomeViewModel> conductor;
        auto model = std::make_unique<MultiView::HomeViewModel>();
        QPointer<MultiView::HomeViewModel> borrowed = model.get();
        conductor.activateItem(std::move(model));
        conductor.activate();
        auto master = createHost(engine, borrowed, QStringLiteral("Master"));
        auto detail = createHost(engine, borrowed, QStringLiteral("Detail"));
        QVERIFY(master && detail && hostItem(master.get()) && hostItem(detail.get()));
        QPointer<QQuickItem> first = hostItem(master.get()), second = hostItem(detail.get());
        bool firstSawModel = false, secondSawModel = false;
        QObject observer;
        connect(first, &QObject::destroyed, &observer, [&] { firstSawModel = bool(borrowed); });
        connect(second, &QObject::destroyed, &observer, [&] { secondSawModel = bool(borrowed); });
        conductor.deactivate(true);
        QVERIFY(!hostItem(master.get()) && !hostItem(detail.get()));
        QTRY_VERIFY(!first && !second && !borrowed);
        QVERIFY(firstSawModel && secondSawModel);
    }

    void modelAndHostDestruction()
    {
        QQmlEngine engine;
        auto model = std::make_unique<MultiView::HomeViewModel>();
        auto first = createHost(engine, model.get(), QStringLiteral("Master"));
        auto second = createHost(engine, model.get(), QStringLiteral("Detail"));
        QVERIFY(first && second && hostItem(first.get()) && hostItem(second.get()));
        first.reset();
        QVERIFY(model && hostItem(second.get()));
        model.reset();
        QVERIFY(!hostItem(second.get()));
        QVERIFY(!second->property("model").value<QObject *>());
    }

    void windowContextParameters_data()
    {
        QTest::addColumn<bool>("throughInterface");
        QTest::addColumn<QVariant>("value");
        for (bool throughInterface : {false, true}) {
            const QByteArray prefix = throughInterface ? "interface-" : "concrete-";
            QTest::newRow((prefix + "default").constData()) << throughInterface << QVariant();
            QTest::newRow((prefix + "master").constData()) << throughInterface << QVariant(QStringLiteral("Master"));
            QTest::newRow((prefix + "empty").constData()) << throughInterface << QVariant(QStringLiteral(""));
        }
    }

    void windowContextParameters()
    {
        QFETCH(bool, throughInterface);
        QFETCH(QVariant, value);
        std::optional<QString> context;
        if (value.isValid()) context = value.toString();
        const bool empty = context && context->isEmpty();
        WindowViewModel primary, auxiliary;
        WindowManager manager;
        WindowCleanup cleanup{manager};
        auto exercise = [&](auto &service) {
            // 显式空 options 后仍可传 context；旧的 (vm, {}) 调用也保持有效。
            QCOMPARE(service.showWindow(QVariant::fromValue(&primary), {}, context), !empty);
            if (empty) QVERIFY(service.showWindow(QVariant::fromValue(&primary), {}));
            QQuickWindow *window = service.windowFor(&primary);
            QVERIFY(window);
            QCOMPARE(window->property("selectedContext"), context && !empty ? *context : QStringLiteral("default"));
            WindowOptions options;
            options.role = WindowRole::Auxiliary;
            options.geometry = QRect(40, 50, 360, 240);
            QCOMPARE(service.showWindow(QVariant::fromValue(&auxiliary), options, context), !empty);
            if (empty) {
                QVERIFY(!service.windowFor(&auxiliary));
            } else {
                QQuickWindow *shown = service.windowFor(&auxiliary);
                QVERIFY(shown);
                QCOMPARE(shown->property("selectedContext"), context ? *context : QStringLiteral("default"));
                QCOMPARE(shown->size(), QSize(360, 240));
            }
            QObject requester;
            for (bool withTitle : {false, true}) {
                DialogOptions dialogOptions;
                if (withTitle) dialogOptions.title = QStringLiteral("独立 context 与标题");
                auto model = std::make_unique<DialogViewModel>();
                QPointer<DialogViewModel> guard = model.get();
                auto future = withTitle
                    ? service.showDialogAsync(std::move(model), &requester, dialogOptions, context)
                    : service.showDialogAsync(std::move(model), &requester, {}, context);
                if (empty) {
                    QTRY_VERIFY(future.isFinished());
                    QVERIFY_THROWS_EXCEPTION(std::exception, future.result());
                } else {
                    auto *dialog = dialogWindow(window);
                    QVERIFY(dialog);
                    QCOMPARE(dialog->title(), dialogOptions.title);
                    QVERIFY(dialog->findChild<QObject *>(context ? "contextDialogItem" : "defaultDialogItem"));
                    guard->tryClose(true);
                    QTRY_VERIFY(future.isFinished());
                    QCOMPARE(future.result(), DialogResult(true));
                }
                QTRY_VERIFY(!guard);
                QVERIFY(!service.busy());
            }
        };
        if (throughInterface) exercise(static_cast<IWindowManager &>(manager));
        else exercise(manager);
    }

    void windowsAndDialogs()
    {
        WindowViewModel primary, auxiliary;
        WindowManager manager;
        WindowCleanup cleanup{manager};
        WindowOptions options;
        QVERIFY(manager.showWindow(QVariant::fromValue(&primary), options, QStringLiteral("Master")));
        auto *window = manager.windowFor(&primary);
        QVERIFY(window);
        QCOMPARE(window->property("selectedContext"), QString("Master"));
        options.role = WindowRole::Auxiliary;
        QVERIFY(manager.showWindow(QVariant::fromValue(&auxiliary), options, QStringLiteral("Master")));
        QCOMPARE(manager.windowFor(&auxiliary)->property("selectedContext"), QString("Master"));
        QVERIFY(!manager.showWindow(QVariant::fromValue(&primary), options, QStringLiteral("Master")));
        QObject requester;
        DialogOptions dialogOptions;
        auto vm = std::make_unique<DialogViewModel>();
        QPointer<DialogViewModel> dialogModel = vm.get();
        auto future = manager.showDialogAsync(std::move(vm), &requester, dialogOptions, QStringLiteral("Master"));
        auto *dialog = dialogWindow(window);
        QVERIFY(dialog && dialog->findChild<QObject *>("contextDialogItem"));
        dialogModel->tryClose(true);
        QTRY_VERIFY(future.isFinished());
        QCOMPARE(future.result(), DialogResult(true));
        QTRY_VERIFY(!dialogModel);
        QVERIFY(!manager.busy());
        auxiliary.allowClose = false;
        auxiliary.tryClose();
        QVERIFY(manager.windowFor(&auxiliary)->isVisible());
        auxiliary.allowClose = true;
        auxiliary.tryClose();
        QTRY_VERIFY(!manager.windowFor(&auxiliary));
    }

    void windowAndDialogFailures()
    {
        WindowViewModel primary, missing;
        WindowManager manager;
        WindowCleanup cleanup{manager};
        QVERIFY(manager.showWindow(QVariant::fromValue(&primary)));
        auto *window = manager.windowFor(&primary);
        QObject requester;
        for (const auto &context : {QStringLiteral("Missing"), QStringLiteral("Broken"), QStringLiteral("")}) {
            WindowOptions options;
            options.role = WindowRole::Auxiliary;
            QVERIFY(!manager.showWindow(QVariant::fromValue(&missing), options, context));
            QVERIFY(!manager.windowFor(&missing));
            QVERIFY(!missing.isInitialized());
            DialogOptions dialogOptions;
            auto model = std::make_unique<DialogViewModel>();
            QPointer<DialogViewModel> guard = model.get();
            auto future = manager.showDialogAsync(std::move(model), &requester, dialogOptions, context);
            QTRY_VERIFY(future.isFinished());
            QVERIFY_THROWS_EXCEPTION(std::exception, future.result());
            QTRY_VERIFY(!guard);
            QVERIFY(!manager.busy() && !dialogWindow(window));
        }
    }

    void example()
    {
        QQmlEngine engine;
        MultiView::ShellViewModel model;
        QQmlComponent component(&engine, ViewLocator::viewUrl(&model));
        std::unique_ptr<QObject> shell(component.createWithInitialProperties({{"viewModel", QVariant::fromValue(&model)}}));
        QVERIFY2(shell, qPrintable(component.errorString()));
        auto *master = shell->findChild<QObject *>("masterHost");
        auto *detail = shell->findChild<QObject *>("contextHost");
        auto *picker = shell->findChild<QObject *>("contextPicker");
        QVERIFY(master && detail && picker && hostItem(master) && hostItem(detail));
        QCOMPARE(hostItem(master)->property("viewModel").value<QObject *>(), model.home());
        QCOMPARE(hostItem(detail)->property("viewModel").value<QObject *>(), model.home());
        model.home()->setName(QStringLiteral("示例同步"));
        QCOMPARE(hostItem(detail)->findChild<QObject *>("nameSummary")->property("text"), QStringLiteral("示例同步"));
        picker->setProperty("currentIndex", 0);
        QCOMPARE(hostItem(detail)->objectName(), QString("masterView"));
        picker->setProperty("currentIndex", 2);
        QCOMPARE(hostItem(detail)->objectName(), QString("defaultView"));
        QVERIFY(hostItem(master));
    }

    void rootContext()
    {
        ContextBootstrapper bootstrapper(*qobject_cast<QGuiApplication *>(QCoreApplication::instance()));
        QCOMPARE(bootstrapper.run(), 0);
        QVERIFY(bootstrapper.contextLoaded && bootstrapper.closed);
    }
};

QTEST_MAIN(MultiViewTests)
#include "tst_multiview.moc"
