#pragma once
#include <CaliburnMicroQt/IInstanceProvider.h>

namespace IoCDetail {
// 实现侧检查应用线程和活动绑定，返回本次调用期间的强引用。
std::shared_ptr<IInstanceProvider> currentProvider(const InstanceRequest &request, InstanceOperation operation);
}

// CM 风格访问门面；绑定由 Bootstrapper 控制，业务代码不设置全局容器。
namespace IoC {
template<class T> std::shared_ptr<T> Get(const QString &name = {})
{
    return IoCDetail::currentProvider({typeid(T), name}, InstanceOperation::Resolve)->resolve<T>(name);
}
template<class T> std::vector<std::shared_ptr<T>> GetAll()
{
    return IoCDetail::currentProvider({typeid(T), {}}, InstanceOperation::ResolveAll)->resolveAll<T>();
}
template<class T> std::unique_ptr<T> Create(const QString &name = {})
{
    return IoCDetail::currentProvider({typeid(T), name}, InstanceOperation::Create)->create<T>(name);
}
}
