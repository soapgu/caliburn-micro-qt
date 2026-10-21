#include "AppBootstrapper.h"
#include <QGuiApplication>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTimer>
#include <QtTest>

Q_IMPORT_QML_PLUGIN(CaliburnMicroQtPlugin)
Q_IMPORT_QML_PLUGIN(CaliburnConsumerModulePlugin)

class ConsumerTests final : public QObject
{
    Q_OBJECT
    QPointer<QQuickWindow> m_window;
    QPointer<ShellViewModel> m_model;

    QQuickWindow *dialog() const
    {
        for (auto *window : QGuiApplication::allWindows())
            if (window->transientParent() == m_window && window->modality() == Qt::ApplicationModal
                    && window->isVisible())
                return qobject_cast<QQuickWindow *>(window);
        return nullptr;
    }
    static bool click(QQuickWindow *window, const char *name)
    {
        auto *item = window ? window->findChild<QQuickItem *>(QString::fromLatin1(name)) : nullptr;
        if (!item || !item->isEnabled() || !item->isVisible())
            return false;
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
            item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint());
        return true;
    }
private slots:
    void initTestCase()
    {
        for (auto *window : QGuiApplication::allWindows())
            if (window->objectName() == QStringLiteral("consumerShell"))
                m_window = qobject_cast<QQuickWindow *>(window);
        QVERIFY(m_window);
        m_model = m_window->property("viewModel").value<ShellViewModel *>();
        QVERIFY(m_model);
        QVERIFY(m_model->isInitialized() && m_model->isActive());
        QCOMPARE(QQmlEngine::objectOwnership(m_model), QQmlEngine::CppOwnership);
        QCOMPARE(ViewLocator::viewUrl(m_model), QUrl(QStringLiteral("qrc:/qt/qml/CaliburnConsumer/views/ShellView.qml")));
        QCOMPARE(ViewLocator::viewUrl(m_model->home()), QUrl(QStringLiteral("qrc:/qt/qml/CaliburnConsumer/views/HomeView.qml")));
        auto *host = m_window->findChild<QQuickItem *>(QStringLiteral("homeHost"));
        QVERIFY(host);
        QTRY_VERIFY(host->property("item").value<QQuickItem *>());
        auto *home = host->property("item").value<QQuickItem *>();
        QCOMPARE(home->property("viewModel").value<HomeViewModel *>(), m_model->home());
        QCOMPARE(host->property("errorString").toString(), QString());

        QPointer<HomeViewModel> homeModel = m_model->home();
        QCOMPARE(homeModel->parent(), m_model.data());
        QCOMPARE(QQmlEngine::objectOwnership(homeModel), QQmlEngine::CppOwnership);
        auto *engine = qmlEngine(m_window.data());
        QVERIFY(engine);
        // 固定成员通过属性 getter 暴露；垃圾回收后仍可继续绑定和交互。
        engine->collectGarbage();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(m_model);
        QVERIFY(homeModel);
        QCOMPARE(m_model->home(), homeModel.data());
        QCOMPARE(home->property("viewModel").value<HomeViewModel *>(), homeModel.data());
    }
    void bindingAndFrameworkConfirmation()
    {
        auto *label = m_window->findChild<QQuickItem *>(QStringLiteral("countLabel"));
        QVERIFY(label);
        QCOMPARE(m_model->home()->count(), 0);
        QVERIFY(click(m_window, "increment"));
        QCOMPARE(m_model->home()->count(), 1);
        QTRY_COMPARE(label->property("text").toString(), QStringLiteral("已点击 1 次"));

        // 分别取消与接受，确认框架内嵌资源、按钮绑定和 Future 结果均可跨模块工作。
        for (const bool accept : {false, true}) {
            QVERIFY(click(m_window, "reset"));
            QTRY_VERIFY(dialog());
            QVERIFY(m_model->resetPending());
            auto *message = dialog()->findChild<QQuickItem *>(QStringLiteral("confirmationMessage"));
            QVERIFY(message);
            QCOMPARE(message->property("text").toString(), QStringLiteral("确认重置计数？"));
            QVERIFY(click(dialog(), accept ? "dialogAccept" : "dialogCancel"));
            QTRY_VERIFY(!m_model->resetPending());
            QCOMPARE(m_model->home()->count(), accept ? 0 : 1);
        }
        QTRY_COMPARE(label->property("text").toString(), QStringLiteral("已点击 0 次"));
    }
    void closeThroughScreen()
    {
        QVERIFY(click(m_window, "closeWindow"));
        QTRY_VERIFY(!m_window || !m_window->isVisible());
        QVERIFY(m_model);
        QVERIFY(!m_model->isActive());
    }
};

class TestBootstrapper final : public AppBootstrapper
{
public:
    using AppBootstrapper::AppBootstrapper;
protected:
    void onStartup() override
    {
        AppBootstrapper::onStartup();
        QTimer::singleShot(0, this, [] {
            ConsumerTests tests;
            QCoreApplication::exit(QTest::qExec(&tests));
        });
    }
};

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    TestBootstrapper bootstrapper(app);
    return bootstrapper.run();
}

#include "tst_consumer.moc"
