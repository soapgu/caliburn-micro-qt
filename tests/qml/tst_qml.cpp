#include <ShellViewModel.h>
#include <ViewModelComposition.h>
#include <CaliburnMicroQt/ViewRegistry.h>
#include <QFile>
#include <QLibraryInfo>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlExtensionPlugin>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QtTest>
#include <memory>

Q_IMPORT_QML_PLUGIN(CaliburnMicroQtPlugin)
Q_IMPORT_QML_PLUGIN(CaliburnExampleModulePlugin)

class UnknownVm : public ViewModelBase { Q_OBJECT };
class MissingResourceVm : public ViewModelBase { Q_OBJECT };
class WrongTypeVm : public ViewModelBase { Q_OBJECT };
class MissingPropertyVm : public ViewModelBase { Q_OBJECT };
class MissingRequiredVm : public ViewModelBase { Q_OBJECT };
class NonVisualVm : public ViewModelBase { Q_OBJECT };
class SyntaxVm : public ViewModelBase { Q_OBJECT };
class MismatchVm : public ViewModelBase { Q_OBJECT };

// 仅允许 Qt 安装模块和内嵌资源，防止测试从构建目录的 QML 副本加载。
static void useEmbeddedModules(QQmlEngine &engine)
{
    engine.setImportPathList({QStringLiteral("qrc:/qt/qml"),
                              QLibraryInfo::path(QLibraryInfo::QmlImportsPath)});
}

static QQuickItem *homeItem(QQuickWindow *window)
{
    auto *host = window->findChild<QQuickItem *>(QStringLiteral("homeHost"));
    return host ? host->property("item").value<QQuickItem *>() : nullptr;
}

static bool displaysHome(QQuickWindow *window, HomeViewModel *model)
{
    auto *item = homeItem(window);
    return item && item->property("viewModel").value<QObject *>() == model;
}

class QmlTests : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QQuickStyle::setStyle(QStringLiteral("Basic"));
        QVERIFY(ViewRegistry::registerView<ShellViewModel>(QUrl(QStringLiteral("qrc:/qt/qml/CaliburnExample/views/ShellView.qml"))));
        QVERIFY(ViewRegistry::registerView<HomeViewModel>(QUrl(QStringLiteral("qrc:/qt/qml/CaliburnExample/views/HomeView.qml"))));
        QVERIFY(ViewRegistry::registerView<MissingResourceVm>(QUrl(QStringLiteral("qrc:/tests/missing.qml"))));
        QVERIFY(ViewRegistry::registerView<WrongTypeVm>(QUrl(QStringLiteral("qrc:/qt/qml/CaliburnExample/views/HomeView.qml"))));
        QVERIFY(ViewRegistry::registerView<MissingPropertyVm>(QUrl(QStringLiteral("qrc:/tests/fixtures/MissingProperty.qml"))));
        QVERIFY(ViewRegistry::registerView<MissingRequiredVm>(QUrl(QStringLiteral("qrc:/tests/fixtures/MissingRequired.qml"))));
        QVERIFY(ViewRegistry::registerView<NonVisualVm>(QUrl(QStringLiteral("qrc:/tests/fixtures/NonVisual.qml"))));
        QVERIFY(ViewRegistry::registerView<SyntaxVm>(QUrl(QStringLiteral("qrc:/tests/fixtures/SyntaxError.qml"))));
        QVERIFY(ViewRegistry::registerView<MismatchVm>(QUrl(QStringLiteral("qrc:/tests/fixtures/Mismatch.qml"))));
        QVERIFY(ViewRegistry::freeze());
    }

    void registeredTypesAreNotCreatable_data()
    {
        QTest::addColumn<QByteArray>("source");
        QTest::newRow("framework") << QByteArray("import Caliburn.Micro.Qt 1.0; ViewModelBase {}");
        QTest::newRow("example") << QByteArray("import CaliburnExample 1.0; ShellViewModel {}");
        QTest::newRow("screen") << QByteArray("import Caliburn.Micro.Qt 1.0; ScreenViewModel {}");
        QTest::newRow("home") << QByteArray("import CaliburnExample 1.0; HomeViewModel {}");
    }

    void registeredTypesAreNotCreatable()
    {
        QFETCH(QByteArray, source);
        QQmlEngine engine;
        useEmbeddedModules(engine);
        QQmlComponent component(&engine);
        component.setData(source, QUrl());
        QVERIFY(component.isError());
        QVERIFY(component.errorString().contains(QStringLiteral("创建")));
    }

    void viewHostStateIsCreatable()
    {
        QQmlEngine engine;
        useEmbeddedModules(engine);
        QQmlComponent component(&engine);
        component.setData("import Caliburn.Micro.Qt 1.0; ViewHostState {}", QUrl());
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        std::unique_ptr<QObject> state(component.create());
        QVERIFY2(state, qPrintable(component.errorString()));
        QVERIFY(!state->property("model").value<QObject *>());
    }

    void requiredTypedInjection()
    {
        HomeViewModel vm;
        QQmlEngine::setObjectOwnership(&vm, QQmlEngine::CppOwnership);
        QQmlEngine engine;
        useEmbeddedModules(engine);
        QQmlComponent missing(&engine);
        missing.setData("import QtQuick; import CaliburnExample 1.0; Item { required property HomeViewModel viewModel }", QUrl());
        QVERIFY2(missing.isReady(), qPrintable(missing.errorString()));
        std::unique_ptr<QObject> absent(missing.create());
        QVERIFY(!absent);
        QVERIFY(missing.errorString().contains(QStringLiteral("Required property")));

        QQmlComponent typed(&engine);
        typed.setData("import QtQuick; import Caliburn.Micro.Qt 1.0; import CaliburnExample 1.0; Item { required property HomeViewModel viewModel; property ViewModelBase base: viewModel }", QUrl());
        QVERIFY2(typed.isReady(), qPrintable(typed.errorString()));
        std::unique_ptr<QObject> object(typed.createWithInitialProperties({{"viewModel", QVariant::fromValue(&vm)}}));
        QVERIFY2(object, qPrintable(typed.errorString()));
        QCOMPARE(object->property("viewModel").value<QObject *>(), &vm);
        QCOMPARE(object->property("base").value<QObject *>(), &vm);
        QCOMPARE(QQmlEngine::objectOwnership(&vm), QQmlEngine::CppOwnership);
    }

    void registrySharedAcrossEngines()
    {
        HomeViewModel home;
        QPointer<HomeViewModel> weak = &home;
        const QUrl expected = ViewRegistry::viewUrl(&home);
        QPointer<ViewRegistry> firstSingleton;
        for (int i = 0; i < 2; ++i) {
            QQmlEngine engine;
            useEmbeddedModules(engine);
            auto *registry = engine.singletonInstance<ViewRegistry *>("Caliburn.Micro.Qt", "ViewRegistry");
            QVERIFY(registry);
            QCOMPARE(registry->resolve(&home), expected);
            if (i == 0)
                firstSingleton = registry;
            else
                QVERIFY(!firstSingleton);
        }
        QVERIFY(weak);
    }

    void hostIdentityAndLifetime()
    {
        auto original = std::make_unique<HomeViewModel>();
        auto replacement = std::make_unique<HomeViewModel>();
        for (auto *vm : {original.get(), replacement.get()})
            QQmlEngine::setObjectOwnership(vm, QQmlEngine::CppOwnership);
        QQmlEngine engine;
        useEmbeddedModules(engine);
        QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        QQmlComponent component(&engine);
        component.setData("import QtQuick; import Caliburn.Micro.Qt 1.0; ViewHost { width: 400; height: 340 }", QUrl());
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        std::unique_ptr<QObject> host(component.createWithInitialProperties({{"model", QVariant::fromValue(original.get())}}));
        QVERIFY2(host, qPrintable(component.errorString()));
        const auto currentItem = [&] { return host->property("item").value<QQuickItem *>(); };
        QVERIFY(currentItem());
        QCOMPARE(currentItem()->property("viewModel").value<QObject *>(), original.get());
        QVERIFY(host->property("errorString").toString().isEmpty());
        QVERIFY(!original->isInitialized());
        QVERIFY(!original->isActive());
        QPointer<QQuickItem> oldItem = currentItem();
        original->add(2);
        QCOMPARE(currentItem(), oldItem.data());
        QCOMPARE(currentItem()->findChild<QQuickItem *>("messageLabel")->property("text").toString(), QStringLiteral("已点击 2 次"));
        QVERIFY(host->setProperty("model", QVariant::fromValue(replacement.get())));
        QVERIFY(currentItem());
        QVERIFY(currentItem() != oldItem.data());
        QCOMPARE(currentItem()->property("viewModel").value<QObject *>(), replacement.get());
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(!oldItem);
        original.reset(); // 旧销毁连接已经断开，不能清空新页面。
        QVERIFY(currentItem());
        QCOMPARE(host->property("model").value<QObject *>(), replacement.get());
        QPointer<QQuickItem> replacedItem = currentItem();
        replacement.reset();
        QVERIFY(!host->property("model").value<QObject *>());
        QVERIFY(!currentItem());
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(!replacedItem);
        HomeViewModel next;
        QQmlEngine::setObjectOwnership(&next, QQmlEngine::CppOwnership);
        QVERIFY(host->setProperty("model", QVariant::fromValue(&next)));
        QVERIFY(currentItem());
        QVERIFY(host->setProperty("model", QVariant::fromValue(static_cast<ViewModelBase *>(nullptr))));
        QVERIFY(!currentItem());
        QVERIFY(host->property("errorString").toString().isEmpty());
        QVERIFY(host->setProperty("model", QVariant::fromValue(&next)));
        QVERIFY(currentItem());
        host.reset();
        QCOMPARE(QQmlEngine::objectOwnership(&next), QQmlEngine::CppOwnership);
        QVERIFY(!next.isInitialized());
        QVERIFY(!next.isActive());
        QCOMPARE(warnings.count(), 0);
    }

    void hostWaitsForCompletion()
    {
        HomeViewModel home;
        QQmlEngine::setObjectOwnership(&home, QQmlEngine::CppOwnership);
        QQmlEngine engine;
        useEmbeddedModules(engine);
        QQmlComponent component(&engine);
        component.setData("import Caliburn.Micro.Qt 1.0; ViewHost {}", QUrl());
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        std::unique_ptr<QObject> host(component.beginCreate(engine.rootContext()));
        QVERIFY(host);
        QVERIFY(host->setProperty("model", QVariant::fromValue(&home)));
        QVERIFY(!host->property("item").value<QObject *>());
        component.completeCreate();
        QVERIFY(host->property("item").value<QQuickItem *>());
        for (const char *property : {"item", "errorString"}) {
            const auto index = host->metaObject()->indexOfProperty(property);
            QVERIFY(index >= 0);
            QVERIFY(!host->metaObject()->property(index).isWritable());
        }
    }

    void shellLifecycleAndHomeDestruction()
    {
        auto assembled = buildShell();
        auto &shell = *assembled;
        auto *home = shell.home();
        QQmlEngine::setObjectOwnership(&shell, QQmlEngine::CppOwnership);
        QQmlEngine::setObjectOwnership(home, QQmlEngine::CppOwnership);
        QQmlApplicationEngine engine;
        useEmbeddedModules(engine);
        QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        engine.setInitialProperties({{"viewModel", QVariant::fromValue(&shell)}});
        engine.load(ViewRegistry::viewUrl(&shell));
        QCOMPARE(engine.rootObjects().size(), 1);
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().front());
        QVERIFY(window);
        auto *label = window->findChild<QQuickItem *>(QStringLiteral("lifecycleLabel"));
        QVERIFY(label && homeItem(window));
        QCOMPARE(label->property("text").toString(), QStringLiteral("Shell：未初始化 / 未激活\nHome：未初始化 / 未激活"));
        shell.activate();
        QTRY_COMPARE(label->property("text").toString(), QStringLiteral("Shell：已初始化 / 已激活\nHome：已初始化 / 已激活"));
        home->deactivate();
        QTRY_COMPARE(label->property("text").toString(), QStringLiteral("Shell：已初始化 / 已激活\nHome：已初始化 / 未激活"));
        QVERIFY(homeItem(window)); // 停用不触发宿主卸载。
        delete home;
        QTRY_VERIFY(!homeItem(window));
        QTRY_COMPARE(label->property("text").toString(), QStringLiteral("Shell：已初始化 / 已激活\nHome：无页面"));
        shell.deactivate(true);
        QCOMPARE(warnings.count(), 0);
        window->close();
    }

    void hostFailurePaths()
    {
        HomeViewModel home;
        ShellViewModel shell(std::make_unique<HomeViewModel>());
        UnknownVm unknown;
        MissingResourceVm missing;
        WrongTypeVm wrong;
        MissingPropertyVm noProperty;
        MissingRequiredVm required;
        NonVisualVm nonVisual;
        SyntaxVm syntax;
        MismatchVm mismatch;
        QQmlEngine engine;
        useEmbeddedModules(engine);
        QQmlComponent component(&engine);
        component.setData("import QtQuick; import Caliburn.Micro.Qt 1.0; ViewHost { width: 400; height: 340 }", QUrl());
        std::unique_ptr<QObject> host(component.create());
        QVERIFY2(host, qPrintable(component.errorString()));
        QQmlEngine::setObjectOwnership(&home, QQmlEngine::CppOwnership);
        for (auto *vm : std::initializer_list<ViewModelBase *>{&unknown, &missing, &wrong, &noProperty,
                                                             &required, &nonVisual, &syntax, &mismatch, &shell}) {
            QQmlEngine::setObjectOwnership(vm, QQmlEngine::CppOwnership);
            QVERIFY(host->setProperty("model", QVariant::fromValue(&home)));
            QVERIFY(host->property("item").value<QQuickItem *>());
            QPointer<QQuickItem> oldItem = host->property("item").value<QQuickItem *>();
            QVERIFY(host->setProperty("model", QVariant::fromValue(vm)));
            QVERIFY(!host->property("item").value<QQuickItem *>());
            QVERIFY(!host->property("errorString").toString().isEmpty());
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            QVERIFY(!oldItem);
        }
        QVERIFY(host->setProperty("model", QVariant::fromValue(&home)));
        QVERIFY(host->property("item").value<QQuickItem *>());
        QVERIFY(host->property("errorString").toString().isEmpty());
        QVERIFY(host->setProperty("model", QVariant::fromValue(static_cast<ViewModelBase *>(nullptr))));
        QVERIFY(!host->property("item").value<QQuickItem *>());
        QVERIFY(host->property("errorString").toString().isEmpty());
    }

    void shellButtonClicks()
    {
        auto shell = buildShell();
        auto *vm = shell->home();
        shell->initialize();
        shell->activate();
        QPointer<HomeViewModel> weak = vm;
        {
            QQmlApplicationEngine engine;
            useEmbeddedModules(engine);
            QSignalSpy warnings(&engine, &QQmlEngine::warnings);
            QVERIFY(QFile::exists(QStringLiteral(":/qt/qml/CaliburnExample/views/ShellView.qml")));
            engine.setInitialProperties({{"viewModel", QVariant::fromValue(shell.get())}});
            engine.load(ViewRegistry::viewUrl(shell.get()));
            QCOMPARE(engine.rootObjects().size(), 1);
            auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().front());
            QVERIFY(window);
            QCOMPARE(window->property("viewModel").value<QObject *>(), shell.get());
            auto *label = window->findChild<QQuickItem *>(QStringLiteral("messageLabel"));
            auto *increase = window->findChild<QQuickItem *>(QStringLiteral("increment"));
            auto *reset = window->findChild<QQuickItem *>(QStringLiteral("reset"));
            QVERIFY(label && increase && reset);
            QVERIFY(QTest::qWaitForWindowExposed(window));
            QCOMPARE(label->property("text").toString(), QStringLiteral("已点击 0 次"));
            QCOMPARE(increase->property("text").toString(), vm->incrementText());
            QTRY_VERIFY(increase->isEnabled());
            QTRY_VERIFY(!reset->isEnabled());
            const auto click = [window](QQuickItem *item) {
                const QPoint position = item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint();
                QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, position);
            };
            for (int i = 1; i <= 5; ++i) {
                click(increase);
                QTRY_COMPARE(vm->count(), i);
                QTRY_COMPARE(label->property("text").toString(), QStringLiteral("已点击 %1 次").arg(i));
            }
            QVERIFY(!increase->isEnabled());
            click(increase);
            QCOMPARE(vm->count(), 5);
            QVERIFY(reset->isEnabled());
            click(reset);
            QTRY_COMPARE(vm->count(), 0);
            QTRY_COMPARE(label->property("text").toString(), QStringLiteral("已点击 0 次"));
            QTRY_VERIFY(increase->isEnabled());
            QTRY_VERIFY(!reset->isEnabled());
            engine.collectGarbage();
            QVERIFY(weak);
            QCOMPARE(warnings.count(), 0);
            window->close();
        }
        QVERIFY(weak);
        shell->deactivate(true);
        shell.reset();
        QVERIFY(!weak);
    }

    void shellBindingsFollowReplacement()
    {
        auto originalTree = buildShell();
        auto replacementTree = buildShell();
        auto &shell = *originalTree;
        auto &replacementShell = *replacementTree;
        auto &original = *shell.home();
        auto &replacement = *replacementShell.home();
        QQmlEngine::setObjectOwnership(&shell, QQmlEngine::CppOwnership);
        QQmlEngine::setObjectOwnership(&replacementShell, QQmlEngine::CppOwnership);
        for (auto *vm : {&original, &replacement})
            QQmlEngine::setObjectOwnership(vm, QQmlEngine::CppOwnership);
        for (int i = 0; i < 5; ++i)
            replacement.increment();

        QQmlApplicationEngine engine;
        useEmbeddedModules(engine);
        QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        engine.setInitialProperties({{"viewModel", QVariant::fromValue(&shell)}});
        engine.load(ViewRegistry::viewUrl(&shell));
        QCOMPARE(engine.rootObjects().size(), 1);
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().front());
        QVERIFY(window);
        auto *label = window->findChild<QQuickItem *>(QStringLiteral("messageLabel"));
        auto *increase = window->findChild<QQuickItem *>(QStringLiteral("increment"));
        auto *reset = window->findChild<QQuickItem *>(QStringLiteral("reset"));
        QVERIFY(label && increase && reset);
        QVERIFY(QTest::qWaitForWindowExposed(window));
        QVERIFY(increase->isEnabled());
        QVERIFY(!reset->isEnabled());
        QVERIFY(window->setProperty("viewModel", QVariant::fromValue(&replacementShell)));
        QTRY_VERIFY(displaysHome(window, &replacement));
        label = homeItem(window)->findChild<QQuickItem *>(QStringLiteral("messageLabel"));
        increase = homeItem(window)->findChild<QQuickItem *>(QStringLiteral("increment"));
        reset = homeItem(window)->findChild<QQuickItem *>(QStringLiteral("reset"));
        QVERIFY(label && increase && reset);
        QTRY_COMPARE(label->property("text").toString(), QStringLiteral("已点击 5 次"));
        QTRY_VERIFY(!increase->isEnabled());
        QTRY_VERIFY(reset->isEnabled());

        // 旧 VM 的通知不能再影响 View，新 VM 的方法仍由真实按钮事件调用。
        original.increment();
        QCOMPARE(label->property("text").toString(), QStringLiteral("已点击 5 次"));
        QVERIFY(!increase->isEnabled());
        const auto click = [window](QQuickItem *item) {
            const QPoint position = item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint();
            QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, position);
        };
        click(reset);
        QTRY_COMPARE(replacement.count(), 0);
        QCOMPARE(original.count(), 1);
        QTRY_VERIFY(increase->isEnabled());
        QTRY_VERIFY(!reset->isEnabled());
        QSignalSpy count(&replacement, &HomeViewModel::countChanged);
        click(increase);
        QTRY_COMPARE(replacement.count(), 1);
        QCOMPARE(count.count(), 1);
        QCOMPARE(original.count(), 1);
        QTRY_COMPARE(label->property("text").toString(), QStringLiteral("已点击 1 次"));
        QCOMPARE(warnings.count(), 0);
        window->close();
    }

    void shellParameterButton()
    {
        auto originalTree = buildShell();
        auto replacementTree = buildShell();
        auto &shell = *originalTree;
        auto &replacementShell = *replacementTree;
        auto &original = *shell.home();
        auto &replacement = *replacementShell.home();
        QQmlEngine::setObjectOwnership(&shell, QQmlEngine::CppOwnership);
        QQmlEngine::setObjectOwnership(&replacementShell, QQmlEngine::CppOwnership);
        QQmlEngine::setObjectOwnership(&original, QQmlEngine::CppOwnership);
        QQmlEngine::setObjectOwnership(&replacement, QQmlEngine::CppOwnership);
        QQmlApplicationEngine engine;
        useEmbeddedModules(engine);
        QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        engine.setInitialProperties({{"viewModel", QVariant::fromValue(&shell)}});
        engine.load(ViewRegistry::viewUrl(&shell));
        QCOMPARE(engine.rootObjects().size(), 1);
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().front());
        QVERIFY(window);
        auto *button = window->findChild<QQuickItem *>(QStringLiteral("addTwo"));
        auto *label = window->findChild<QQuickItem *>(QStringLiteral("messageLabel"));
        QVERIFY(button && label);
        QVERIFY(QTest::qWaitForWindowExposed(window));
        const auto click = [window, &button] {
            QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                             button->mapToScene(QPointF(button->width() / 2, button->height() / 2)).toPoint());
        };
        QSignalSpy originalCount(&original, &HomeViewModel::countChanged);
        QVERIFY(button->isEnabled());
        click();
        QTRY_COMPARE(original.count(), 2);
        QCOMPARE(originalCount.count(), 1);
        click();
        QTRY_COMPARE(original.count(), 4);
        QTRY_COMPARE(label->property("text").toString(), QStringLiteral("已点击 4 次"));
        QVERIFY(!button->isEnabled());
        click();
        QCOMPARE(original.count(), 4);
        QCOMPARE(originalCount.count(), 2);

        QVERIFY(window->setProperty("viewModel", QVariant::fromValue(&replacementShell)));
        QTRY_VERIFY(displaysHome(window, &replacement));
        button = homeItem(window)->findChild<QQuickItem *>(QStringLiteral("addTwo"));
        label = homeItem(window)->findChild<QQuickItem *>(QStringLiteral("messageLabel"));
        QVERIFY(button && label);
        QTRY_VERIFY(button->isEnabled());
        original.increment();
        QVERIFY(button->isEnabled());
        QSignalSpy replacementCount(&replacement, &HomeViewModel::countChanged);
        click();
        QTRY_COMPARE(replacement.count(), 2);
        QCOMPARE(replacementCount.count(), 1);
        QCOMPARE(original.count(), 5);
        replacement.increment();
        QVERIFY(button->isEnabled()); // 3 + 2 恰好到上限。
        click();
        QTRY_COMPARE(replacement.count(), 5);
        QVERIFY(!button->isEnabled());
        replacement.reset();
        QTRY_VERIFY(button->isEnabled());
        QCOMPARE(warnings.count(), 0);
        window->close();
    }

    void shellKeyboardAndFocus()
    {
        auto originalTree = buildShell();
        auto replacementTree = buildShell();
        auto &shell = *originalTree;
        auto &replacementShell = *replacementTree;
        auto &original = *shell.home();
        auto &replacement = *replacementShell.home();
        QQmlEngine::setObjectOwnership(&shell, QQmlEngine::CppOwnership);
        QQmlEngine::setObjectOwnership(&replacementShell, QQmlEngine::CppOwnership);
        QQmlEngine::setObjectOwnership(&original, QQmlEngine::CppOwnership);
        QQmlEngine::setObjectOwnership(&replacement, QQmlEngine::CppOwnership);
        QQmlApplicationEngine engine;
        useEmbeddedModules(engine);
        QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        engine.setInitialProperties({{"viewModel", QVariant::fromValue(&shell)}});
        engine.load(ViewRegistry::viewUrl(&shell));
        QCOMPARE(engine.rootObjects().size(), 1);
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().front());
        QVERIFY(window);
        auto *page = window->findChild<QQuickItem *>(QStringLiteral("inputScope"));
        auto *input = window->findChild<QQuickItem *>(QStringLiteral("focusInput"));
        auto *increase = window->findChild<QQuickItem *>(QStringLiteral("increment"));
        auto *label = window->findChild<QQuickItem *>(QStringLiteral("messageLabel"));
        auto *addTwo = window->findChild<QQuickItem *>(QStringLiteral("addTwo"));
        auto *reset = window->findChild<QQuickItem *>(QStringLiteral("reset"));
        QVERIFY(page && input && increase && label && addTwo && reset);
        QVERIFY(QTest::qWaitForWindowExposed(window));
        QTRY_COMPARE(window->activeFocusItem(), page);
        const auto press = [window](int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier,
                                    bool repeat = false) {
            QKeyEvent event(QEvent::KeyPress, key, modifiers, QString::number(2), repeat);
            QCoreApplication::sendEvent(window, &event);
            const bool accepted = event.isAccepted();
            QKeyEvent release(QEvent::KeyRelease, key, modifiers, QString::number(2), repeat);
            QCoreApplication::sendEvent(window, &release);
            return accepted;
        };
        QSignalSpy count(&original, &HomeViewModel::countChanged);
        QVERIFY(!press(Qt::Key_2, Qt::NoModifier, true));
        QVERIFY(!press(Qt::Key_2, Qt::ControlModifier));
        QVERIFY(!press(Qt::Key_2, Qt::ShiftModifier));
        QVERIFY(!press(Qt::Key_3));
        QCOMPARE(original.count(), 0);
        QVERIFY(press(Qt::Key_2));
        QCOMPARE(original.count(), 2);
        QCOMPARE(count.count(), 1);
        QVERIFY(!press(Qt::Key_2, Qt::NoModifier, true));
        QCOMPARE(original.count(), 2);
        QVERIFY(press(Qt::Key_2));
        QCOMPARE(original.count(), 4);
        QVERIFY(!press(Qt::Key_2));
        QCOMPARE(original.count(), 4);
        QCOMPARE(count.count(), 2);

        original.reset();
        const auto click = [window](QQuickItem *item) {
            QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                             item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint());
        };
        click(input);
        QTRY_VERIFY(input->hasActiveFocus());
        QTest::keyClick(window, Qt::Key_2);
        QCOMPARE(input->property("text").toString(), QStringLiteral("2"));
        QCOMPARE(original.count(), 0);
        QTest::keyClick(window, Qt::Key_Backspace);
        QCOMPARE(input->property("text").toString(), QString());
        QCOMPARE(original.count(), 0);
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, page->mapToScene(QPointF(8, 8)).toPoint());
        QTRY_COMPARE(window->activeFocusItem(), page);
        QTest::keyClick(window, Qt::Key_Tab);
        QTRY_VERIFY(increase->hasActiveFocus());
        QTest::keyClick(window, Qt::Key_Space);
        QCOMPARE(original.count(), 1); // 按钮原生空格操作继续可用。
        QTest::keyClick(window, Qt::Key_2);
        QCOMPARE(original.count(), 3); // 按钮未消费的 2 向页面传播。
        QTest::keyClick(window, Qt::Key_Tab);
        QTRY_VERIFY(addTwo->hasActiveFocus());
        QTest::keyClick(window, Qt::Key_Tab);
        QTRY_VERIFY(reset->hasActiveFocus());
        QTest::keyClick(window, Qt::Key_Tab);
        QTRY_VERIFY(input->hasActiveFocus());
        QTest::keyClick(window, Qt::Key_Backtab);
        QTRY_VERIFY(reset->hasActiveFocus());
        QTest::keyClick(window, Qt::Key_Backtab);
        QTRY_VERIFY(addTwo->hasActiveFocus());
        original.increment(); // 加 2 禁用，Tab 应跳过它。
        increase->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_Tab);
        QTRY_VERIFY(reset->hasActiveFocus());
        increase->forceActiveFocus();

        QVERIFY(window->setProperty("viewModel", QVariant::fromValue(&replacementShell)));
        QTRY_VERIFY(displaysHome(window, &replacement));
        page = homeItem(window)->findChild<QQuickItem *>(QStringLiteral("inputScope"));
        label = homeItem(window)->findChild<QQuickItem *>(QStringLiteral("messageLabel"));
        QVERIFY(page && label);
        QTRY_COMPARE(window->activeFocusItem(), page);
        QSignalSpy replacementCount(&replacement, &HomeViewModel::countChanged);
        QTest::keyClick(window, Qt::Key_2);
        QCOMPARE(replacement.count(), 2);
        QCOMPARE(replacementCount.count(), 1);
        QCOMPARE(original.count(), 4);
        original.increment();
        QTRY_COMPARE(label->property("text").toString(), QStringLiteral("已点击 2 次"));
        QCOMPARE(warnings.count(), 0);
        window->close();
    }
};

QTEST_MAIN(QmlTests)
#include "tst_qml.moc"
