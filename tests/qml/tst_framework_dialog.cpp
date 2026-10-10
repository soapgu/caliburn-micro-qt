#include "../support/DialogWindowSupport.h"
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

static std::unique_ptr<ConfirmActionViewModel> confirmation(WindowManager &manager)
{
    return std::make_unique<ConfirmActionViewModel>(
        ConfirmationRequest{QStringLiteral("测试"), QStringLiteral("确认操作")}, manager);
}

class FrameworkDialogTests : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase() {
        QQuickStyle::setStyle(QStringLiteral("Basic"));
        QVERIFY(ViewRegistry::registerView<ScreenViewModel>(QUrl("qrc:/tests/fixtures/FrameworkWindow.qml")));
    }

    void registryMapping()
    {
        ScreenViewModel root;
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
        auto *window = showManagedWindow(manager, &root);
        QVERIFY(window);
        QObject requester;
        auto *vm = model.get();
        auto future = manager.showDialogAsync(std::move(model), &requester);
        if (mode == "missing") {
            QTRY_VERIFY(future.isFinished());
            QVERIFY_THROWS_EXCEPTION(std::runtime_error, future.result());
            QVERIFY(!manager.busy());
            return;
        }
        auto *host = dialogWindow(window);
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

    void managedWindowsKeepBusinessLayout()
    {
        ScreenViewModel firstModel, secondModel, rejectedModel;
        WindowManager manager, other;
        auto *first = showManagedWindow(manager, &firstModel);
        auto *second = showManagedWindow(other, &secondModel);
        QVERIFY(first && second && first != second);
        auto *layout = first->findChild<QQuickItem *>(QStringLiteral("businessLayout"));
        QVERIFY(layout);
        const QSizeF businessSize(layout->width(), layout->height());
        QVERIFY(!first->findChild<QQuickItem *>(QStringLiteral("dialogHost")));
        QVERIFY(!manager.showWindow(QVariant::fromValue(&rejectedModel)));
        QVERIFY(!rejectedModel.isInitialized());
        QObject requester;
        auto future = manager.showDialogAsync(confirmation(manager), &requester);
        QVERIFY(dialogWindow(first));
        QVERIFY(!dialogWindow(second) && !other.busy());
        QCOMPARE(QSizeF(layout->width(), layout->height()), businessSize);
        manager.cancelDialogsFor(&requester);
        QCOMPARE(future.result(), DialogResult{});
        QCOMPARE(QSizeF(layout->width(), layout->height()), businessSize);
        manager.releaseWindows();
        QVERIFY(second->isVisible() && secondModel.isActive());
    }

    void oldManualHostIsUnavailable()
    {
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.setData("import Caliburn.Micro.Qt 1.0; DialogHostState {}", QUrl());
        QVERIFY(component.isError());
    }

    void destructionCompletesRequest_data()
    {
        QTest::addColumn<QString>("kind");
        for (const auto *kind : {"prepare", "release", "window", "manager", "requester"})
            QTest::newRow(kind) << QString::fromLatin1(kind);
    }

    void destructionCompletesRequest()
    {
        QFETCH(QString, kind);
        ScreenViewModel root;
        auto manager = std::make_unique<WindowManager>();
        QPointer<QQuickWindow> window = showManagedWindow(*manager, &root);
        auto requester = std::make_unique<QObject>();
        QVERIFY(window);
        auto model = confirmation(*manager);
        QPointer<ConfirmActionViewModel> weak = model.get();
        auto future = manager->showDialogAsync(std::move(model), requester.get());
        auto *host = dialogWindow(window);
        QVERIFY(host);
        QPointer<QQuickItem> view = host->property("dialogItem").value<QQuickItem *>();
        QVERIFY(view);
        QStringList order;
        connect(view, &QObject::destroyed, this, [&] { order << "view"; });
        connect(weak, &QObject::destroyed, this, [&] { order << "vm"; });
        if (kind == "prepare") manager->prepareForShutdown();
        if (kind == "release") manager->releaseWindows();
        if (kind == "window") delete window.data();
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
