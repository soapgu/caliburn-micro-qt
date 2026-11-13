#pragma once

#include <QString>
#include <exception>
#include <memory>
#include <stdexcept>
#include <typeindex>
#include <vector>

// 绑定错误的诊断元数据；模板入口使用空名称，不参与类型分派。
struct InstanceRequest
{
    std::type_index type;
    QString name;
};

// 保留诊断编号，历史枚举值不代表当前提供对应能力。
enum class InstanceOperation { Resolve = 0, Create = 2, ResolveAll = 3, Register = 5 };
enum class InstanceErrorCode {
    NotRegistered, TypeMismatch, DuplicateContract, CircularDependency,
    UnsupportedCapability, CreationFailed
};

class InstanceError : public std::runtime_error
{
public:
    InstanceError(InstanceErrorCode code, InstanceOperation operation, InstanceRequest request,
                  QString reason, std::vector<InstanceRequest> dependencyChain = {});
    InstanceErrorCode code() const noexcept { return m_code; }
    InstanceOperation operation() const noexcept { return m_operation; }
    const InstanceRequest &request() const noexcept { return m_request; }
    const std::vector<InstanceRequest> &dependencyChain() const noexcept { return m_chain; }
private:
    InstanceErrorCode m_code;
    InstanceOperation m_operation;
    InstanceRequest m_request;
    std::vector<InstanceRequest> m_chain;
};

class IWindowManager;

// Bootstrapper 只依赖固定窗口服务契约；任意类型的创建由模板层负责。
class IInstanceProvider
{
public:
    virtual ~IInstanceProvider() = default;
    virtual std::shared_ptr<IWindowManager> windowManager() = 0;
};
