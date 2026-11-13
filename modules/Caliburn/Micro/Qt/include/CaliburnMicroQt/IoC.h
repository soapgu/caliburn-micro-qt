#pragma once
#include <CaliburnMicroQt/IInstanceProvider.h>

namespace IoCDetail {
// 实现侧检查应用线程和活动绑定，返回本次调用期间的强引用。
std::shared_ptr<IInstanceProvider> currentProvider(const InstanceRequest &request, InstanceOperation operation);
}

// 应用将其别名为 IoC；Provider 必须与 Bootstrapper 安装的具体类型一致。
// 复用唯一全局绑定，类型在编译期确定，不做运行时请求分派。
template<class Provider>
class TypedIoC
{
public:
    template<class T> static std::shared_ptr<T> Get()
    {
        return std::static_pointer_cast<Provider>(IoCDetail::currentProvider(
            {typeid(T), {}}, InstanceOperation::Resolve))->template resolve<T>();
    }

    template<class T> static std::unique_ptr<T> Create()
    {
        return std::static_pointer_cast<Provider>(IoCDetail::currentProvider(
            {typeid(T), {}}, InstanceOperation::Create))->template create<T>();
    }
};
