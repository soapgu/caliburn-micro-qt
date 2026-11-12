#include "../support/TestInstanceProvider.h"
#include <QtTest>

namespace {
struct Service { int value = 0; };
struct Dependency {};
struct Left { virtual ~Left() = default; int padding = 7; };
struct Contract { int value = 42; }; // 故意没有虚析构函数。
struct Implementation : Left, Contract {
    explicit Implementation(int &destroyed) : destroyed(destroyed) {}
    ~Implementation() override { ++destroyed; }
    int &destroyed;
};
struct Other {
    explicit Other(int &destroyed) : destroyed(destroyed) {}
    ~Other() { ++destroyed; }
    int &destroyed;
};
class SharedOnlyProvider : public IInstanceProvider {
    std::optional<SharedInstance> resolveShared(const InstanceRequest &) override { return std::nullopt; }
};
template<class Action>
InstanceError failure(Action action)
{
    try { action(); }
    catch (const InstanceError &error) { return error; }
    throw std::logic_error("预期 InstanceError，但操作成功");
}
}

class InstanceProviderTests : public QObject
{
    Q_OBJECT
private slots:
    void sharedIdentityAndNames()
    {
        TestInstanceProvider provider;
        auto original = std::make_shared<Service>();
        auto named = std::make_shared<Service>();
        provider.add({typeid(Service), {}}, [original] { return SharedInstance::from<Service>(original); });
        provider.add({typeid(Service), "A"}, [named] { return SharedInstance::from<Service>(named); });
        auto result = provider.resolve<Service>();
        QCOMPARE(result.get(), original.get());
        QVERIFY(!result.owner_before(original) && !original.owner_before(result));
        QCOMPARE(provider.resolve<Service>("A"), named);
        QVERIFY(!provider.tryResolve<Service>("a"));
        QVERIFY(!provider.tryResolve<Service>(" A"));
        QVERIFY(!provider.tryResolve<Dependency>());
        const auto error = failure([&] { provider.resolve<Dependency>("missing"); });
        QCOMPARE(error.code(), InstanceErrorCode::NotRegistered);
        QCOMPARE(error.operation(), InstanceOperation::Resolve);
        QVERIFY(error.request().type == typeid(Dependency));
        QCOMPARE(error.request().name, QString("missing"));
        QVERIFY(QString::fromUtf8(error.what()).contains("missing"));
    }
    void sharedInterfaceAdjustmentAndLifetime()
    {
        int destroyed = 0;
        std::shared_ptr<Contract> result;
        std::weak_ptr<Implementation> weak;
        {
            TestInstanceProvider provider;
            auto original = std::make_shared<Implementation>(destroyed);
            weak = original;
            QVERIFY(static_cast<void *>(original.get()) != static_cast<void *>(static_cast<Contract *>(original.get())));
            provider.add({typeid(Contract), {}}, [original] { return SharedInstance::from<Contract>(original); });
            result = provider.resolve<Contract>();
            QCOMPARE(result.get(), static_cast<Contract *>(original.get()));
            QVERIFY(!result.owner_before(original) && !original.owner_before(result));
        }
        QCOMPARE(destroyed, 0);
        QVERIFY(!weak.expired());
        result.reset();
        QVERIFY(weak.expired());
        QCOMPARE(destroyed, 1);
    }
    void uniqueAdjustmentMoveAndLifetime()
    {
        static_assert(!std::is_copy_constructible_v<UniqueInstance>);
        static_assert(std::is_nothrow_move_constructible_v<UniqueInstance>);
        int destroyed = 0;
        OwnedInstance<Contract> result;
        Contract *expected = nullptr;
        bool adjusted = false;
        {
            TestInstanceProvider provider;
            provider.uniqueFactory = [&](const InstanceRequest &) {
                auto object = std::make_unique<Implementation>(destroyed);
                expected = object.get();
                adjusted = static_cast<void *>(object.get()) != static_cast<void *>(expected);
                return UniqueInstance::from<Contract>(std::move(object));
            };
            result = provider.create<Contract>();
            QCOMPARE(result.get(), expected);
            QVERIFY(adjusted);
            QCOMPARE(result->value, 42);
            auto next = provider.create<Contract>();
            QVERIFY(next.get() != result.get());
            auto moved = std::move(next);
            QVERIFY(!next);
            auto replacement = UniqueInstance::from<Contract>(std::make_unique<Implementation>(destroyed));
            replacement = UniqueInstance::from<Contract>(std::make_unique<Implementation>(destroyed));
            QCOMPARE(destroyed, 1);
        }
        QCOMPARE(destroyed, 3);
        result.reset();
        QCOMPARE(destroyed, 4);
    }
    void wrongTypesAndEmptyResults()
    {
        TestInstanceProvider provider;
        provider.add({typeid(Service), "wrong"}, [] { return SharedInstance::from<Dependency>(std::make_shared<Dependency>()); });
        provider.add({typeid(Service), "empty"}, [] { return SharedInstance::from<Service>(std::shared_ptr<Service>{}); });
        for (bool optional : {false, true}) {
            auto request = [&](const char *name) {
                if (optional) provider.tryResolve<Service>(name);
                else provider.resolve<Service>(name);
            };
            QCOMPARE(failure([&] { request("wrong"); }).code(), InstanceErrorCode::TypeMismatch);
            QCOMPARE(failure([&] { request("empty"); }).code(), InstanceErrorCode::CreationFailed);
        }
        int destroyed = 0;
        provider.uniqueFactory = [&](const InstanceRequest &) {
            return UniqueInstance::from<Other>(std::make_unique<Other>(destroyed));
        };
        QCOMPARE(failure([&] { provider.create<Service>(); }).code(), InstanceErrorCode::TypeMismatch);
        QCOMPARE(destroyed, 1);
        provider.uniqueFactory = [](const InstanceRequest &) { return UniqueInstance::from<Service>(std::unique_ptr<Service>{}); };
        QCOMPARE(failure([&] { provider.create<Service>(); }).code(), InstanceErrorCode::CreationFailed);
    }
    void collectionOrderAndValidation()
    {
        TestInstanceProvider provider;
        QVERIFY(provider.resolveAll<Service>().empty());
        auto first = std::make_shared<Service>();
        auto second = std::make_shared<Service>();
        provider.add({typeid(Service), "named"}, [first] { return SharedInstance::from<Service>(first); });
        provider.add({typeid(Service), {}}, [second] { return SharedInstance::from<Service>(second); });
        provider.append({typeid(Service), "named"}, [first] { return SharedInstance::from<Service>(first); });
        auto all = provider.resolveAll<Service>();
        QCOMPARE(all.size(), size_t(3));
        QCOMPARE(all[0], first); QCOMPARE(all[1], second); QCOMPARE(all[2], first);
        provider.append({typeid(Service), {}}, [] { return SharedInstance::from<Dependency>(std::make_shared<Dependency>()); });
        QCOMPARE(failure([&] { provider.resolveAll<Service>(); }).code(), InstanceErrorCode::TypeMismatch);
        provider.entries.pop_back();
        provider.append({typeid(Service), {}}, [] { return SharedInstance::from<Service>(std::shared_ptr<Service>{}); });
        QCOMPARE(failure([&] { provider.resolveAll<Service>(); }).code(), InstanceErrorCode::CreationFailed);
    }
    void buildUpAndUnsupportedCapabilities()
    {
        TestInstanceProvider provider;
        Service object;
        provider.injection = [&](const InstanceRequest &request, BorrowedInstance borrowed) {
            QVERIFY(request.type == typeid(Service));
            QCOMPARE(&borrowed.as<Service>(), &object);
            borrowed.as<Service>().value = 9;
        };
        provider.buildUp(object);
        QCOMPARE(object.value, 9);
        provider.injection = [](const InstanceRequest &, BorrowedInstance borrowed) { borrowed.as<Dependency>(); };
        QCOMPARE(failure([&] { provider.buildUp(object); }).code(), InstanceErrorCode::TypeMismatch);
        SharedOnlyProvider minimal;
        QVERIFY(!minimal.tryResolve<Service>());
        QCOMPARE(failure([&] { minimal.create<Service>(); }).code(), InstanceErrorCode::UnsupportedCapability);
        QCOMPARE(failure([&] { minimal.resolveAll<Service>(); }).code(), InstanceErrorCode::UnsupportedCapability);
        QCOMPARE(failure([&] { minimal.buildUp(object); }).code(), InstanceErrorCode::UnsupportedCapability);
    }
    void duplicatesCyclesAndDependencyErrors()
    {
        TestInstanceProvider provider;
        provider.add({typeid(Service), {}}, [&] {
            provider.resolve<Dependency>("reader");
            return SharedInstance::from<Service>(std::make_shared<Service>());
        });
        const auto missing = failure([&] { provider.tryResolve<Service>(); });
        QCOMPARE(missing.code(), InstanceErrorCode::NotRegistered);
        QVERIFY(missing.request().type == typeid(Dependency));
        QCOMPARE(missing.dependencyChain().size(), size_t(2));
        QVERIFY(missing.dependencyChain().front().type == typeid(Service));
        QCOMPARE(missing.dependencyChain().back().name, QString("reader"));
        const auto duplicate = failure([&] {
            provider.add({typeid(Service), {}}, [] { return SharedInstance::from<Service>(std::make_shared<Service>()); });
        });
        QCOMPARE(duplicate.code(), InstanceErrorCode::DuplicateContract);
        QCOMPARE(duplicate.operation(), InstanceOperation::Register);
        provider.add({typeid(Dependency), "reader"}, [&] {
            provider.resolve<Service>();
            return SharedInstance::from<Dependency>(std::make_shared<Dependency>());
        });
        const auto cycle = failure([&] { provider.resolve<Service>(); });
        QCOMPARE(cycle.code(), InstanceErrorCode::CircularDependency);
        QCOMPARE(cycle.dependencyChain().size(), size_t(3));
        QVERIFY(cycle.dependencyChain().front().type == cycle.dependencyChain().back().type);
        // 栈必须在异常后清空，不能把下一次正常解析误判为循环。
        provider.entries[1].factory = [] { return SharedInstance::from<Dependency>(std::make_shared<Dependency>()); };
        QVERIFY(provider.resolve<Service>());
    }
    void preservesAdapterError()
    {
        TestInstanceProvider provider;
        const InstanceRequest dependency{typeid(Dependency), "inner"};
        const std::vector<InstanceRequest> chain{{typeid(Service), "outer"}, dependency};
        const InstanceError original(InstanceErrorCode::NotRegistered, InstanceOperation::Resolve,
                                     dependency, QStringLiteral("原始错误"), chain);
        provider.uniqueFactory = [&](const InstanceRequest &) -> UniqueInstance { throw original; };
        const auto error = failure([&] { provider.create<Service>("outer"); });
        QCOMPARE(error.code(), original.code());
        QCOMPARE(error.operation(), original.operation());
        QVERIFY(error.request().type == dependency.type);
        QCOMPARE(error.request().name, dependency.name);
        QCOMPARE(QString::fromUtf8(error.what()), QString::fromUtf8(original.what()));
        QCOMPARE(error.dependencyChain().size(), chain.size());
        for (size_t i = 0; i < chain.size(); ++i) {
            QVERIFY(error.dependencyChain()[i].type == chain[i].type);
            QCOMPARE(error.dependencyChain()[i].name, chain[i].name);
        }
    }
    void exceptionsAndContextIsolation()
    {
        TestInstanceProvider provider;
        provider.add({typeid(Service), {}}, []() -> SharedInstance { throw std::logic_error("adapter failure"); });
        try {
            provider.tryResolve<Service>();
            QFAIL("预期适配器异常");
        } catch (const InstanceError &error) {
            QCOMPARE(error.code(), InstanceErrorCode::CreationFailed);
            QCOMPARE(error.operation(), InstanceOperation::TryResolve);
            bool nested = false;
            try { std::rethrow_if_nested(error); }
            catch (const std::logic_error &cause) { nested = std::string(cause.what()) == "adapter failure"; }
            QVERIFY(nested);
        }
        provider.uniqueFactory = [](const InstanceRequest &) -> UniqueInstance { throw 17; };
        try {
            provider.create<Service>();
            QFAIL("预期未知异常");
        } catch (const InstanceError &error) {
            QCOMPARE(error.code(), InstanceErrorCode::CreationFailed);
            bool nested = false;
            try { std::rethrow_if_nested(error); } catch (int value) { nested = value == 17; }
            QVERIFY(nested);
        }
        provider.injection = [](const InstanceRequest &, BorrowedInstance) { throw std::runtime_error("inject"); };
        Service object;
        QCOMPARE(failure([&] { provider.buildUp(object); }).operation(), InstanceOperation::BuildUp);
        QCOMPARE(failure([&] { provider.resolveAll<Service>(); }).operation(), InstanceOperation::ResolveAll);
        TestInstanceProvider other;
        QVERIFY(!other.tryResolve<Service>());
        auto local = std::make_shared<Service>();
        other.add({typeid(Service), {}}, [local] { return SharedInstance::from<Service>(local); });
        QCOMPARE(other.resolve<Service>(), local);
        QCOMPARE(failure([&] { provider.resolve<Service>(); }).code(), InstanceErrorCode::CreationFailed);
    }
};
QTEST_GUILESS_MAIN(InstanceProviderTests)
#include "tst_instanceprovider.moc"
