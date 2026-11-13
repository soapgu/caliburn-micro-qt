#include <CaliburnMicroQt/IoC.h>
#include "IoCBinding.h"
#include "../support/TestInstanceProvider.h"
#include <QtTest>
#include <thread>

namespace {
using IoC = TypedIoC<TestInstanceProvider>;
struct Value { int number = 7; };
template<class Action> InstanceError errorOf(Action action)
{
    try { action(); } catch (const InstanceError &error) { return error; }
    throw std::logic_error("预期 InstanceError");
}
}
class IoCTests : public QObject
{
    Q_OBJECT
private slots:
    void unavailableAndMetadata() {
        const auto error = errorOf([] { IoC::Create<Value>(); });
        QCOMPARE(error.code(), InstanceErrorCode::CreationFailed);
        QCOMPARE(error.operation(), InstanceOperation::Create);
        QVERIFY(error.request().type == typeid(Value));
        QVERIFY(error.request().name.isEmpty());
        QCOMPARE(errorOf([] { IoC::Get<Value>(); }).operation(), InstanceOperation::Resolve);
    }
    void forwardingAndUnbinding() {
        TestInjector injector;
        const auto value = injector.marker;
        bool fail = false;
        injector.beforeCreate = [&] { if (fail) throw std::logic_error("原始依赖异常"); };
        auto provider = std::make_shared<TestInstanceProvider>(std::move(injector));
        std::shared_ptr<int> retained;
        {
            IoCDetail::Binding binding(provider);
            retained = IoC::Get<int>();
            QCOMPARE(retained, value);
            QCOMPARE(retained, provider->resolve<int>());
            QVERIFY(!retained.owner_before(value) && !value.owner_before(retained));
            auto first = IoC::Create<Value>();
            static_assert(std::is_same_v<decltype(first), std::unique_ptr<Value>>);
            auto second = IoC::Create<Value>();
            QVERIFY(first && second && first.get() != second.get());
            QCOMPARE(first->number, 7);
            fail = true;
            try { IoC::Create<Value>(); QFAIL("构造异常必须传播"); }
            catch (const std::logic_error &error) {
                QCOMPARE(QString::fromUtf8(error.what()), QStringLiteral("原始依赖异常"));
            }
            fail = false;
            QVERIFY(IoC::Create<Value>());
        }
        QCOMPARE(retained, value);
        QCOMPARE(errorOf([] { IoC::Get<int>(); }).code(), InstanceErrorCode::CreationFailed);
    }
    void expiredProviderAndConflict() {
        auto provider = std::make_shared<TestInstanceProvider>();
        auto other = std::make_shared<TestInstanceProvider>();
        {
            IoCDetail::Binding binding(provider);
            QVERIFY_THROWS_EXCEPTION(std::logic_error, IoCDetail::Binding{provider});
            QVERIFY_THROWS_EXCEPTION(std::logic_error, IoCDetail::Binding{other});
            provider.reset();
            QCOMPARE(errorOf([] { IoC::Get<Value>(); }).code(), InstanceErrorCode::CreationFailed);
            QVERIFY_THROWS_EXCEPTION(std::logic_error, IoCDetail::Binding{other});
        }
        IoCDetail::Binding next(other);
        QCOMPARE(IoC::Get<int>(), other->resolve<int>());
    }
    void threadAndExceptionalScope() {
        auto provider = std::make_shared<TestInstanceProvider>();
        bool rejectedGet = false, rejectedCreate = false, rejectedBinding = false;
        {
            IoCDetail::Binding binding(provider);
            std::thread worker([&] {
                rejectedGet = errorOf([] { IoC::Get<Value>(); }).code() == InstanceErrorCode::CreationFailed;
                rejectedCreate = errorOf([] { IoC::Create<Value>(); }).code() == InstanceErrorCode::CreationFailed;
            });
            worker.join();
            QVERIFY(rejectedGet && rejectedCreate);
        }
        std::thread bindingWorker([&] {
            try { IoCDetail::Binding invalid(provider); }
            catch (const std::logic_error &) { rejectedBinding = true; }
        });
        bindingWorker.join();
        QVERIFY(rejectedBinding);
        try { IoCDetail::Binding binding(provider); throw std::runtime_error("scope"); }
        catch (const std::runtime_error &) {}
        QCOMPARE(errorOf([] { IoC::Get<Value>(); }).code(), InstanceErrorCode::CreationFailed);
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, IoCDetail::Binding(std::shared_ptr<IInstanceProvider>{}));
    }
};
QTEST_GUILESS_MAIN(IoCTests)
#include "tst_ioc.moc"
