#include "../support/TestInstanceProvider.h"
#include <CaliburnMicroQt/WindowManager.h>
#include <QtTest>

namespace {
struct Unlisted { int value = 7; };
struct Counted {
    inline static int alive = 0;
    Counted() { ++alive; }
    ~Counted() { --alive; }
};
}

class InstanceProviderTests : public QObject
{
    Q_OBJECT
private slots:
    void sharedIdentityAndFixedContract() {
        TestInjector injector;
        injector.windows = std::make_shared<WindowManager>();
        const auto marker = injector.marker;
        const auto windows = injector.windows;
        auto provider = std::make_shared<TestInstanceProvider>(std::move(injector));
        QCOMPARE(provider->resolve<int>(), marker);
        QCOMPARE(provider->resolve<int>(), provider->resolve<int>());
        std::shared_ptr<IInstanceProvider> contract = provider;
        QCOMPARE(contract->windowManager(), windows);
        QCOMPARE(provider->resolve<IWindowManager>(), windows);
    }
    void arbitraryTypesAndOwnership() {
        std::unique_ptr<Counted> retained;
        {
            TestInstanceProvider provider;
            auto shared = provider.resolve<Unlisted>();
            auto first = provider.create<Unlisted>();
            auto second = provider.create<Unlisted>();
            static_assert(std::is_same_v<decltype(first), std::unique_ptr<Unlisted>>);
            QCOMPARE(shared->value, 7);
            QVERIFY(first.get() != second.get());
            retained = provider.create<Counted>();
            QCOMPARE(Counted::alive, 1);
        }
        QCOMPARE(Counted::alive, 1);
        retained.reset();
        QCOMPARE(Counted::alive, 0);
    }
    void rawExceptionsAndContextIsolation() {
        TestInjector injector;
        bool fail = true;
        injector.beforeCreate = [&] { if (fail) throw std::logic_error("原始构造异常"); };
        TestInstanceProvider provider(std::move(injector));
        QVERIFY_THROWS_EXCEPTION(std::logic_error, provider.create<Unlisted>());
        QVERIFY_THROWS_EXCEPTION(std::logic_error, provider.resolve<int>());
        fail = false;
        QVERIFY(provider.create<Unlisted>());
        TestInstanceProvider other;
        QVERIFY(provider.resolve<int>() != other.resolve<int>());
    }
};
QTEST_GUILESS_MAIN(InstanceProviderTests)
#include "tst_instanceprovider.moc"
