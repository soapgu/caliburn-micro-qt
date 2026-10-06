#include <ShellViewModel.h>
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

// 仅允许 Qt 安装模块和内嵌资源，防止测试从构建目录的 QML 副本加载。
static void useEmbeddedModules(QQmlEngine &engine)
{
    engine.setImportPathList({QStringLiteral("qrc:/qt/qml"),
                              QLibraryInfo::path(QLibraryInfo::QmlImportsPath)});
}

class QmlTests : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QQuickStyle::setStyle(QStringLiteral("Basic"));
    }

    void registeredTypesAreNotCreatable_data()
    {
        QTest::addColumn<QByteArray>("source");
        QTest::newRow("framework") << QByteArray("import Caliburn.Micro.Qt 1.0; ViewModelBase {}");
        QTest::newRow("example") << QByteArray("import CaliburnExample 1.0; ShellViewModel {}");
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

    void requiredTypedInjection()
    {
        ShellViewModel vm;
        QQmlEngine::setObjectOwnership(&vm, QQmlEngine::CppOwnership);
        QQmlEngine engine;
        useEmbeddedModules(engine);
        QQmlComponent missing(&engine);
        missing.setData("import QtQuick; import CaliburnExample 1.0; Item { required property ShellViewModel viewModel }", QUrl());
        QVERIFY2(missing.isReady(), qPrintable(missing.errorString()));
        std::unique_ptr<QObject> absent(missing.create());
        QVERIFY(!absent);
        QVERIFY(missing.errorString().contains(QStringLiteral("Required property")));

        QQmlComponent typed(&engine);
        typed.setData("import QtQuick; import Caliburn.Micro.Qt 1.0; import CaliburnExample 1.0; Item { required property ShellViewModel viewModel; property ViewModelBase base: viewModel }", QUrl());
        QVERIFY2(typed.isReady(), qPrintable(typed.errorString()));
        std::unique_ptr<QObject> object(typed.createWithInitialProperties({{"viewModel", QVariant::fromValue(&vm)}}));
        QVERIFY2(object, qPrintable(typed.errorString()));
        QCOMPARE(object->property("viewModel").value<QObject *>(), &vm);
        QCOMPARE(object->property("base").value<QObject *>(), &vm);
        QCOMPARE(QQmlEngine::objectOwnership(&vm), QQmlEngine::CppOwnership);
    }

    void shellButtonClicks()
    {
        auto vm = std::make_unique<ShellViewModel>();
        QPointer<ShellViewModel> weak = vm.get();
        QQmlEngine::setObjectOwnership(vm.get(), QQmlEngine::CppOwnership);
        {
            QQmlApplicationEngine engine;
            useEmbeddedModules(engine);
            QSignalSpy warnings(&engine, &QQmlEngine::warnings);
            QVERIFY(QFile::exists(QStringLiteral(":/qt/qml/CaliburnExample/views/ShellView.qml")));
            engine.setInitialProperties({{"viewModel", QVariant::fromValue(vm.get())}});
            engine.load(QUrl(QStringLiteral("qrc:/qt/qml/CaliburnExample/views/ShellView.qml")));
            QCOMPARE(engine.rootObjects().size(), 1);
            auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().front());
            QVERIFY(window);
            QCOMPARE(window->property("viewModel").value<QObject *>(), vm.get());
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
        vm.reset();
        QVERIFY(!weak);
    }

    void shellBindingsFollowReplacement()
    {
        ShellViewModel original, replacement;
        for (auto *vm : {&original, &replacement})
            QQmlEngine::setObjectOwnership(vm, QQmlEngine::CppOwnership);
        for (int i = 0; i < 5; ++i)
            replacement.increment();

        QQmlApplicationEngine engine;
        useEmbeddedModules(engine);
        QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        engine.setInitialProperties({{"viewModel", QVariant::fromValue(&original)}});
        engine.load(QUrl(QStringLiteral("qrc:/qt/qml/CaliburnExample/views/ShellView.qml")));
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
        QVERIFY(window->setProperty("viewModel", QVariant::fromValue(&replacement)));
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
        QSignalSpy count(&replacement, &ShellViewModel::countChanged);
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
        ShellViewModel original, replacement;
        QQmlEngine::setObjectOwnership(&original, QQmlEngine::CppOwnership);
        QQmlEngine::setObjectOwnership(&replacement, QQmlEngine::CppOwnership);
        QQmlApplicationEngine engine;
        useEmbeddedModules(engine);
        QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        engine.setInitialProperties({{"viewModel", QVariant::fromValue(&original)}});
        engine.load(QUrl(QStringLiteral("qrc:/qt/qml/CaliburnExample/views/ShellView.qml")));
        QCOMPARE(engine.rootObjects().size(), 1);
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().front());
        QVERIFY(window);
        auto *button = window->findChild<QQuickItem *>(QStringLiteral("addTwo"));
        auto *label = window->findChild<QQuickItem *>(QStringLiteral("messageLabel"));
        QVERIFY(button && label);
        QVERIFY(QTest::qWaitForWindowExposed(window));
        const auto click = [window, button] {
            QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                             button->mapToScene(QPointF(button->width() / 2, button->height() / 2)).toPoint());
        };
        QSignalSpy originalCount(&original, &ShellViewModel::countChanged);
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

        QVERIFY(window->setProperty("viewModel", QVariant::fromValue(&replacement)));
        QTRY_VERIFY(button->isEnabled());
        original.increment();
        QVERIFY(button->isEnabled());
        QSignalSpy replacementCount(&replacement, &ShellViewModel::countChanged);
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
        ShellViewModel original, replacement;
        QQmlEngine::setObjectOwnership(&original, QQmlEngine::CppOwnership);
        QQmlEngine::setObjectOwnership(&replacement, QQmlEngine::CppOwnership);
        QQmlApplicationEngine engine;
        useEmbeddedModules(engine);
        QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        engine.setInitialProperties({{"viewModel", QVariant::fromValue(&original)}});
        engine.load(QUrl(QStringLiteral("qrc:/qt/qml/CaliburnExample/views/ShellView.qml")));
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
        QSignalSpy count(&original, &ShellViewModel::countChanged);
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
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, QPoint(8, 8));
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

        QVERIFY(window->setProperty("viewModel", QVariant::fromValue(&replacement)));
        QSignalSpy replacementCount(&replacement, &ShellViewModel::countChanged);
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
