#pragma once
#include <CaliburnMicroQt/InstanceProvider.h>
#include <functional>
#include <type_traits>

// 测试用强类型后端，固定依赖之外的类型直接默认构造，不维护运行时类型表。
struct TestInjector
{
    std::shared_ptr<IWindowManager> windows;
    std::shared_ptr<int> marker = std::make_shared<int>(42);
    std::function<void()> beforeCreate;

    template<class Pointer> Pointer create()
    {
        if (beforeCreate) beforeCreate();
        if constexpr (std::is_same_v<Pointer, std::shared_ptr<IWindowManager>>)
            return windows;
        else if constexpr (std::is_same_v<Pointer, std::shared_ptr<int>>)
            return marker;
        else
            return Pointer(new typename Pointer::element_type());
    }
};

class TestInstanceProvider : public InstanceProvider<TestInjector>
{
public:
    explicit TestInstanceProvider(TestInjector injector = {})
        : InstanceProvider<TestInjector>(std::move(injector)) {}
};
