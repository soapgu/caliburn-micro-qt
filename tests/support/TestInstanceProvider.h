#pragma once
#include <CaliburnMicroQt/IInstanceProvider.h>
#include <algorithm>

// 仅供契约测试使用的显式注册适配器，不随框架发布。
class TestInstanceProvider : public IInstanceProvider
{
public:
    using SharedFactory = std::function<SharedInstance()>;
    struct Entry { InstanceRequest request; SharedFactory factory; bool single; };
    std::vector<Entry> entries;
    std::function<UniqueInstance(const InstanceRequest &)> uniqueFactory;
    std::function<void(const InstanceRequest &, BorrowedInstance)> injection;

    void add(InstanceRequest request, SharedFactory factory)
    {
        for (const auto &entry : entries)
            if (entry.single && equal(entry.request, request))
                throw InstanceError(InstanceErrorCode::DuplicateContract, InstanceOperation::Register,
                                    request, QStringLiteral("单实例契约重复"));
        entries.push_back({std::move(request), std::move(factory), true});
    }
    void append(InstanceRequest request, SharedFactory factory)
    { entries.push_back({std::move(request), std::move(factory), false}); }

protected:
    std::optional<SharedInstance> resolveShared(const InstanceRequest &request) override
    {
        for (const auto &entry : entries) {
            if (!entry.single || !equal(entry.request, request)) continue;
            if (std::any_of(m_chain.begin(), m_chain.end(), [&](const auto &step) { return equal(step, request); })) {
                auto chain = m_chain;
                chain.push_back(request);
                throw InstanceError(InstanceErrorCode::CircularDependency, InstanceOperation::Resolve,
                                    request, QStringLiteral("循环依赖"), std::move(chain));
            }
            m_chain.push_back(request);
            struct Pop { std::vector<InstanceRequest> &chain; ~Pop() { chain.pop_back(); } } pop{m_chain};
            try { return entry.factory(); }
            catch (const InstanceError &error) {
                // 最内层依赖缺失附带当前创建栈；已有依赖链保持原样。
                if (!error.dependencyChain().empty()) throw;
                auto chain = m_chain;
                chain.push_back(error.request());
                throw InstanceError(error.code(), error.operation(), error.request(),
                                    QString::fromUtf8(error.what()), std::move(chain));
            }
        }
        return std::nullopt;
    }
    UniqueInstance createUnique(const InstanceRequest &request) override
    {
        if (uniqueFactory) return uniqueFactory(request);
        return IInstanceProvider::createUnique(request);
    }
    std::vector<SharedInstance> resolveCollection(const InstanceRequest &request) override
    {
        std::vector<SharedInstance> result;
        for (const auto &entry : entries)
            if (entry.request.type == request.type) result.push_back(entry.factory());
        return result;
    }
    void injectExisting(const InstanceRequest &request, BorrowedInstance object) override
    {
        if (injection) injection(request, object);
        else IInstanceProvider::injectExisting(request, object);
    }
private:
    static bool equal(const InstanceRequest &a, const InstanceRequest &b)
    { return a.type == b.type && a.name == b.name; }
    std::vector<InstanceRequest> m_chain;
};
