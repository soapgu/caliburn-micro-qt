#pragma once
#include <CaliburnMicroQt/IInstanceProvider.h>
#include <utility>

// Injector 只需提供 create<T>()，框架不依赖具体 DI 库。
template<class Injector>
class InstanceProvider : public IInstanceProvider
{
public:
    explicit InstanceProvider(Injector injector) : m_injector(std::move(injector)) {}

    template<class T> std::shared_ptr<T> resolve()
    { return m_injector.template create<std::shared_ptr<T>>(); }

    template<class T> std::unique_ptr<T> create()
    { return m_injector.template create<std::unique_ptr<T>>(); }

    std::shared_ptr<IWindowManager> windowManager() override
    { return resolve<IWindowManager>(); }

private:
    Injector m_injector;
};
