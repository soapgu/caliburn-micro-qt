#pragma once
#include <CaliburnMicroQt/IInstanceProvider.h>
#include <functional>

// 仅供边界测试使用的预设结果替身，不实现依赖图、作用域或循环检测。
class TestInstanceProvider : public IInstanceProvider
{
public:
    using SharedFactory = std::function<SharedInstance()>;
    struct Entry { InstanceRequest request; SharedFactory factory; };
    std::vector<Entry> entries;
    std::function<UniqueInstance(const InstanceRequest &)> uniqueFactory;
    std::function<std::vector<SharedInstance>(const InstanceRequest &)> collectionFactory;

    void add(InstanceRequest request, SharedFactory factory)
    { entries.push_back({std::move(request), std::move(factory)}); }

protected:
    std::optional<SharedInstance> resolveShared(const InstanceRequest &request) override
    {
        for (const auto &entry : entries)
            if (entry.request.type == request.type && entry.request.name == request.name)
                return entry.factory();
        return std::nullopt;
    }
    UniqueInstance createUnique(const InstanceRequest &request) override
    {
        if (uniqueFactory) return uniqueFactory(request);
        return IInstanceProvider::createUnique(request);
    }
    std::vector<SharedInstance> resolveCollection(const InstanceRequest &request) override
    {
        if (collectionFactory) return collectionFactory(request);
        return IInstanceProvider::resolveCollection(request);
    }
};
