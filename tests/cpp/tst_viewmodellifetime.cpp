#include <CaliburnMicroQt/Conductor.h>
#include "ViewHostState.h"
#include <QtTest>
#include <memory>

// 模拟 View 的子对象在析构中仍读取 VM，验证整个 View 子树都先于 VM 释放。
class ViewChildProbe final : public QObject
{
public:
    ViewChildProbe(QObject *view, QObject *model, bool &alive)
        : QObject(view), model(model), alive(alive) {}
    ~ViewChildProbe() override { alive = bool(model); }
private:
    QPointer<QObject> model;
    bool &alive;
};

class ViewModelLifetimeTests : public QObject
{
    Q_OBJECT
private slots:
    void multipleViewsAndEarlyOwnerDestruction_data()
    {
        QTest::addColumn<bool>("collection");
        QTest::addColumn<bool>("destroyOwner");
        QTest::newRow("single-close") << false << false;
        QTest::newRow("collection-close") << true << false;
        QTest::newRow("single-destroy") << false << true;
        QTest::newRow("collection-destroy") << true << true;
    }

    void multipleViewsAndEarlyOwnerDestruction()
    {
        QFETCH(bool, collection);
        QFETCH(bool, destroyOwner);
        std::unique_ptr<ConductorBaseWithActiveItem> owner;
        if (collection) {
            auto c = std::make_unique<Conductor<>::Collection::OneActive>();
            c->activateItem(std::make_unique<QObject>());
            owner = std::move(c);
        } else {
            auto c = std::make_unique<Conductor<>>();
            c->activateItem(std::make_unique<QObject>());
            owner = std::move(c);
        }
        owner->activate();
        QPointer<QObject> model = owner->activeItem();
        QSignalSpy destroyed(model, &QObject::destroyed);
        ViewHostState firstHost, secondHost;
        firstHost.setModel(model);
        secondHost.setModel(model);
        auto firstView = std::make_unique<QObject>();
        auto secondView = std::make_unique<QObject>();
        bool firstChildSawModel = false, secondChildSawModel = false;
        new ViewChildProbe(firstView.get(), model, firstChildSawModel);
        new ViewChildProbe(secondView.get(), model, secondChildSawModel);
        firstHost.retainView(firstView.get());
        secondHost.retainView(secondView.get());
        if (destroyOwner) owner.reset();
        else owner->deactivate(true);
        QVERIFY(model && !model->parent());
        QVERIFY(!firstHost.model() && !secondHost.model());
        firstHost.setModel(model); // 待回收对象不能被重新装配。
        QVERIFY(!firstHost.model());
        if (owner) {
            owner->activateItem(model.data());
            QVERIFY(!owner->activeItem());
            owner.reset();
        }
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(model);
        firstView.reset();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(model && firstChildSawModel);
        secondView.reset();
        QVERIFY(model && secondChildSawModel);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(!model);
        QCOMPARE(destroyed.count(), 1);
    }

    void closingInactiveMemberReleasesItsIndependentView()
    {
        Conductor<>::Collection::OneActive c;
        c.activateItem(std::make_unique<QObject>());
        QPointer<QObject> old = c.activeItem();
        ViewHostState host;
        host.setModel(old);
        auto view = std::make_unique<QObject>();
        host.retainView(view.get());
        c.activateItem(std::make_unique<QObject>());
        auto *current = c.activeItem();
        c.closeItem(old);
        QCOMPARE(c.activeItem(), current);
        QCOMPARE(c.items(), QList<QObject *>{current});
        QVERIFY(!host.model());
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(old);
        view.reset();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(!old);
        QCOMPARE(c.activeItem(), current);
    }
};

QTEST_GUILESS_MAIN(ViewModelLifetimeTests)
#include "tst_viewmodellifetime.moc"
