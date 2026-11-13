#include <CaliburnMicroQt/IoC.h>
#include "IoCBinding.h"
#include <QCoreApplication>
#include <QThread>

namespace IoCDetail {
struct BindingIdentity {};
namespace {
struct Context {
    std::weak_ptr<IInstanceProvider> provider;
    std::weak_ptr<BindingIdentity> identity;
};
Context &context() { static Context value; return value; }
bool applicationThread()
{
    auto *app = QCoreApplication::instance();
    return app && QThread::currentThread() == app->thread();
}
}
Binding::Binding(const std::shared_ptr<IInstanceProvider> &provider)
{
    if (!applicationThread()) throw std::logic_error("IoC binding requires the application thread");
    if (!provider) throw std::invalid_argument("IoC provider must not be null");
    auto &state = context();
    if (!state.identity.expired()) throw std::logic_error("An IoC provider is already bound");
    m_identity = std::make_shared<BindingIdentity>();
    state.provider = provider;
    state.identity = m_identity;
}
Binding::~Binding()
{
    auto &state = context();
    if (state.identity.lock() == m_identity) {
        state.provider.reset();
        state.identity.reset();
    }
}
std::shared_ptr<IInstanceProvider> currentProvider(const InstanceRequest &request, InstanceOperation operation)
{
    // 错误线程不读取无锁的应用线程上下文。
    if (!applicationThread())
        throw InstanceError(InstanceErrorCode::CreationFailed, operation, request,
                            QStringLiteral("IoC 请求必须在应用线程执行"));
    auto &state = context();
    if (state.identity.expired())
        throw InstanceError(InstanceErrorCode::CreationFailed, operation, request,
                            QStringLiteral("IoC 尚未绑定或已经解绑"));
    auto provider = state.provider.lock();
    if (!provider)
        throw InstanceError(InstanceErrorCode::CreationFailed, operation, request,
                            QStringLiteral("IoC Provider 已失效"));
    return provider;
}
}
