#pragma once
#include <memory>
class IInstanceProvider;
namespace IoCDetail {
struct BindingIdentity;
// 仅供框架内部使用。每个凭据独占一个绑定身份，不覆盖其他活动上下文。
class Binding final
{
public:
    explicit Binding(const std::shared_ptr<IInstanceProvider> &provider);
    ~Binding();
    Binding(const Binding &) = delete;
    Binding &operator=(const Binding &) = delete;
private:
    std::shared_ptr<BindingIdentity> m_identity;
};
}
