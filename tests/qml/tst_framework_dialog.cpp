#include "../support/DialogWindowSupport.h"
#include <CaliburnMicroQt/ConfirmActionViewModel.h>
#include <CaliburnMicroQt/ViewLocator.h>
#include <CaliburnMicroQt/WindowManager.h>
#include <QDir>
#include <QFile>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QThread>
#include <QtTest>
#include <memory>
#include <stdexcept>

Q_IMPORT_QML_PLUGIN(CaliburnMicroQtPlugin)

static std::unique_ptr<ConfirmActionViewModel> confirmation()
{
    return std::make_unique<ConfirmActionViewModel>(
        ConfirmationRequest{QStringLiteral("测试"), QStringLiteral("确认操作")});
}

class FrameworkDialogTests : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase() {
        QQuickStyle::setStyle(QStringLiteral("Basic"));
        QVERIFY(ViewLocator::registerView<ScreenViewModel>(QUrl("qrc:/tests/fixtures/FrameworkWindow.qml")));
    }

    void registryMapping()
    {
        ScreenViewModel root;
        WindowManager manager;
        auto model = confirmation();
        const QUrl defaultUrl(QStringLiteral("qrc:/qt/qml/Caliburn/Micro/Qt/ConfirmActionView.qml"));
        QCOMPARE(ViewLocator::viewUrl(model.get()), defaultUrl); // 无 Bootstrapper、无显式登记。
        const QString mode = qEnvironmentVariable("CMQT_CONFIRM_MAPPING");
        QUrl selected = defaultUrl;
        QString expectedName;
        QTemporaryDir brokenRoot;
        QVERIFY(brokenRoot.isValid());
        const QUrl applicationRoot(QStringLiteral("qrc:/confirm-override/"));
        if (mode == "missingRoot") {
            QVERIFY(ViewLocator::addSearchRoot({QUrl("qrc:/absent-root/")}));
        } else if (mode == "broken" || mode == "wrongType") {
            // 文件存在且已选定，装配失败必须结束请求，不能再使用框架候选。
            QVERIFY(QDir(brokenRoot.path()).mkdir(QStringLiteral("views")));
            QFile file(brokenRoot.filePath(QStringLiteral("views/ConfirmActionView.qml")));
            QVERIFY(file.open(QIODevice::WriteOnly));
            const QByteArray source = mode == "broken" ? QByteArray("import QtQuick; Item { invalid syntax")
                : QByteArray("import QtQuick; import Caliburn.Micro.Qt 1.0; Item { required property WindowManager viewModel }");
            QCOMPARE(file.write(source), qint64(source.size()));
            file.close();
            QVERIFY(ViewLocator::addSearchRoot({QUrl::fromLocalFile(brokenRoot.path())}));
            selected = QUrl::fromLocalFile(file.fileName());
        } else if (!mode.isEmpty()) {
            if (mode == "restricted") {
                QVERIFY(ViewLocator::addSearchRoot({applicationRoot, QStringLiteral("Application")}));
                QCOMPARE(ViewLocator::viewUrl(model.get()), defaultUrl); // 全局类型不匹配应用前缀。
            }
            const QUrl searchRoot = mode == "flat"
                ? QUrl(QStringLiteral("qrc:/confirm-override/views/")) : applicationRoot;
            QVERIFY(ViewLocator::addSearchRoot({searchRoot}));
            selected = QUrl(QStringLiteral("qrc:/confirm-override/views/ConfirmActionView.qml"));
            QCOMPARE(ViewLocator::viewUrl(model.get()), selected);
            expectedName = QStringLiteral("conventionConfirm");
        }
        if (mode == "override" || mode == "missing") {
            selected = QUrl(mode == "missing" ? QStringLiteral("qrc:/tests/absent-confirm.qml")
                : QStringLiteral("qrc:/tests/fixtures/OverrideConfirm.qml"));
            expectedName = QStringLiteral("overrideConfirm");
            QVERIFY(ViewLocator::registerView<ConfirmActionViewModel>(selected));
            QVERIFY(ViewLocator::registerView<ConfirmActionViewModel>(selected));
            QTest::ignoreMessage(QtWarningMsg, "ViewLocator: conflicting type mapping ConfirmActionViewModel");
            QVERIFY(!ViewLocator::registerView<ConfirmActionViewModel>(defaultUrl));
        }
        QCOMPARE(ViewLocator::viewUrl(model.get()), selected);
        QVERIFY(ViewLocator::freeze());
        QTest::ignoreMessage(QtWarningMsg, "ViewLocator: registration rejected after freeze");
        QVERIFY(!ViewLocator::registerView<ConfirmActionViewModel>(defaultUrl));
        auto *window = showManagedWindow(manager, &root);
        QVERIFY(window);
        QObject requester;
        auto *vm = model.get();
        auto future = manager.showDialogAsync(std::move(model), &requester);
        if (mode == "missing" || mode == "broken" || mode == "wrongType") {
            QTRY_VERIFY(future.isFinished());
            QVERIFY_THROWS_EXCEPTION(std::runtime_error, future.result());
            QVERIFY(!manager.busy());
            return;
        }
        auto *host = dialogWindow(window);
        QVERIFY(host);
        auto *view = host->property("dialogItem").value<QQuickItem *>();
        QVERIFY(view);
        if (!expectedName.isEmpty())
            QCOMPARE(view->objectName(), expectedName);
        vm->accept();
        QTRY_VERIFY(future.isFinished());
        QCOMPARE(future.result(), DialogResult(true));
        auto cancelModel = confirmation();
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
        auto future = manager.showDialogAsync(confirmation(), &requester);
        QVERIFY(dialogWindow(first));
        QVERIFY(!dialogWindow(second) && !other.busy());
        QCOMPARE(QSizeF(layout->width(), layout->height()), businessSize);
        qobject_cast<ScreenViewModel *>(manager.currentDialog())->tryClose();
        QTRY_VERIFY(future.isFinished());
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
        auto model = confirmation();
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
