#include <CaliburnMicroQt/IoC.h>
#include "IoCBinding.h"
#include "../support/TestInstanceProvider.h"
#include <QtTest>
#include <thread>

namespace {
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
        const auto error = errorOf([] { IoC::Create<Value>("example"); });
        QCOMPARE(error.code(), InstanceErrorCode::CreationFailed);
        QCOMPARE(error.operation(), InstanceOperation::Create);
        QVERIFY(error.request().type == typeid(Value));
        QCOMPARE(error.request().name, QString("example"));
        QCOMPARE(errorOf([] { IoC::Get<Value>(); }).code(), InstanceErrorCode::CreationFailed);
    }
    void collectionForwardingAndUnbinding() {
        const auto unavailable = errorOf([] { IoC::GetAll<Value>(); });
        QCOMPARE(unavailable.code(), InstanceErrorCode::CreationFailed);
        QCOMPARE(unavailable.operation(), InstanceOperation::ResolveAll);
        QVERIFY(unavailable.request().type == typeid(Value));
        QVERIFY(unavailable.request().name.isEmpty());
        auto provider = std::make_shared<TestInstanceProvider>();
        auto first = std::make_shared<Value>();
        auto second = std::make_shared<Value>();
        std::vector<std::shared_ptr<Value>> retained;
        {
            IoCDetail::Binding binding(provider);
            QCOMPARE(errorOf([] { IoC::GetAll<Value>(); }).code(), InstanceErrorCode::UnsupportedCapability);
            provider->collectionFactory = [](const InstanceRequest &) { return std::vector<SharedInstance>{}; };
            QVERIFY(IoC::GetAll<Value>().empty());
            provider->collectionFactory = [first, second](const InstanceRequest &) {
                return std::vector<SharedInstance>{SharedInstance::from<Value>(first),
                    SharedInstance::from<Value>(second), SharedInstance::from<Value>(first)};
            };
            retained = IoC::GetAll<Value>();
            static_assert(std::is_same_v<decltype(retained), std::vector<std::shared_ptr<Value>>>);
            QCOMPARE(retained.size(), size_t(3));
            QCOMPARE(retained[0], first); QCOMPARE(retained[1], second); QCOMPARE(retained[2], first);
            QVERIFY(!retained[0].owner_before(first) && !first.owner_before(retained[0]));
            const InstanceError original(InstanceErrorCode::CircularDependency, InstanceOperation::ResolveAll,
                                         {typeid(Value), {}}, QStringLiteral("集合后端错误"));
            provider->collectionFactory = [&](const InstanceRequest &) -> std::vector<SharedInstance> { throw original; };
            const auto error = errorOf([] { IoC::GetAll<Value>(); });
            QCOMPARE(error.code(), original.code());
            QCOMPARE(error.operation(), original.operation());
            QCOMPARE(QString::fromUtf8(error.what()), QString::fromUtf8(original.what()));
        }
        QCOMPARE(errorOf([] { IoC::GetAll<Value>(); }).code(), InstanceErrorCode::CreationFailed);
        QCOMPARE(retained[0], first);
    }
    void forwardingAndUnbinding() {
        auto provider = std::make_shared<TestInstanceProvider>();
        auto value = std::make_shared<Value>();
        provider->add({typeid(Value), "named"}, [value] { return SharedInstance::from<Value>(value); });
        QString createdName;
        provider->uniqueFactory = [&](const InstanceRequest &request) {
            createdName = request.name;
            return UniqueInstance::from<Value>(std::make_unique<Value>());
        };
        {
            IoCDetail::Binding binding(provider);
            auto resolved = IoC::Get<Value>("named");
            QCOMPARE(resolved, value);
            QVERIFY(!resolved.owner_before(value) && !value.owner_before(resolved));
            auto first = IoC::Create<Value>("page");
            static_assert(std::is_same_v<decltype(first), std::unique_ptr<Value>>);
            auto second = IoC::Create<Value>("page");
            QVERIFY(first && second && first.get() != second.get());
            QCOMPARE(createdName, QString("page"));
            QCOMPARE(errorOf([] { IoC::Get<Value>(); }).code(), InstanceErrorCode::NotRegistered);
            provider->uniqueFactory = [](const InstanceRequest &request) -> UniqueInstance {
                throw InstanceError(InstanceErrorCode::CircularDependency, InstanceOperation::Create,
                                    request, QStringLiteral("原始依赖错误"), {request});
            };
            const auto error = errorOf([] { IoC::Create<Value>("cycle"); });
            QCOMPARE(error.code(), InstanceErrorCode::CircularDependency);
            QCOMPARE(error.dependencyChain().size(), size_t(1));
            QCOMPARE(error.request().name, QString("cycle"));
        }
        // 外部强引用仍存在，门面也不能访问已解除的上下文。
        QVERIFY(provider);
        QCOMPARE(errorOf([] { IoC::Get<Value>("named"); }).code(), InstanceErrorCode::CreationFailed);
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
            QCOMPARE(errorOf([] { IoC::GetAll<Value>(); }).code(), InstanceErrorCode::CreationFailed);
            // 凭据仍活动时，失效的弱 Provider 也不能被静默覆盖。
            QVERIFY_THROWS_EXCEPTION(std::logic_error, IoCDetail::Binding{other});
        }
        IoCDetail::Binding next(other);
        QCOMPARE(errorOf([] { IoC::Get<Value>(); }).code(), InstanceErrorCode::NotRegistered);
    }
    void threadAndExceptionalScope() {
        auto provider = std::make_shared<TestInstanceProvider>();
        bool rejectedGet = false, rejectedCreate = false, rejectedAll = false, rejectedBinding = false;
        {
            IoCDetail::Binding binding(provider);
            std::thread worker([&] {
                rejectedGet = errorOf([] { IoC::Get<Value>(); }).code() == InstanceErrorCode::CreationFailed;
                rejectedAll = errorOf([] { IoC::GetAll<Value>(); }).code() == InstanceErrorCode::CreationFailed;
                rejectedCreate = errorOf([] { IoC::Create<Value>(); }).code() == InstanceErrorCode::CreationFailed;
            });
            worker.join();
            QVERIFY(rejectedGet && rejectedCreate && rejectedAll);
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
