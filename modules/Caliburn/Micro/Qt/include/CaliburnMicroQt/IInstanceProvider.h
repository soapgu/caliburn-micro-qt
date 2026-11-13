#pragma once

#include <QString>
#include <exception>
#include <memory>
#include <optional>
#include <stdexcept>
#include <typeindex>
#include <type_traits>
#include <utility>
#include <vector>

// 名称按原文区分；空名称表示未命名契约。类型身份仅用于当前进程。
struct InstanceRequest
{
    std::type_index type;
    QString name;
};

// 保留现有诊断操作编号；已移除的操作不再提供枚举项。
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

namespace InstanceDetail {
template<class T> constexpr void checkType()
{
    static_assert(std::is_object_v<T> && !std::is_const_v<T> && !std::is_volatile_v<T>
                  && !std::is_pointer_v<T> && !std::is_array_v<T>,
                  "契约必须是非 cv、非指针、非数组的对象类型");
}
void validate(std::type_index actual, bool present, const InstanceRequest &request,
              InstanceOperation operation);
}

class IInstanceProvider;

class SharedInstance
{
public:
    template<class Contract, class Implementation>
    static SharedInstance from(std::shared_ptr<Implementation> object)
    {
        InstanceDetail::checkType<Contract>();
        std::shared_ptr<Contract> contract = std::move(object);
        return SharedInstance(typeid(Contract), std::move(contract));
    }
private:
    friend class IInstanceProvider;
    SharedInstance(std::type_index type, std::shared_ptr<void> object)
        : m_type(type), m_object(std::move(object)) {}
    std::type_index m_type;
    std::shared_ptr<void> m_object;
};

class UniqueInstance
{
public:
    UniqueInstance(UniqueInstance &&) noexcept = default;
    UniqueInstance &operator=(UniqueInstance &&) noexcept = default;
    UniqueInstance(const UniqueInstance &) = delete;
    UniqueInstance &operator=(const UniqueInstance &) = delete;

    // 仅支持默认删除；跨类型映射必须能通过契约虚析构安全释放。
    template<class Contract, class Implementation>
    static UniqueInstance from(std::unique_ptr<Implementation> object)
    {
        InstanceDetail::checkType<Contract>();
        InstanceDetail::checkType<Implementation>();
        static_assert(std::is_convertible_v<Implementation *, Contract *>, "独占契约指针必须可转换");
        static_assert(std::is_destructible_v<Contract>, "独占契约必须可析构");
        static_assert(std::is_same_v<Contract, Implementation> || std::has_virtual_destructor_v<Contract>,
                      "跨类型独占映射要求契约具有虚析构函数");
        std::unique_ptr<Contract> contract = std::move(object);
        return UniqueInstance(typeid(Contract), std::make_unique<Holder<Contract>>(std::move(contract)));
    }
private:
    friend class IInstanceProvider;
    struct HolderBase {
        virtual ~HolderBase() = default;
        virtual bool present() const noexcept = 0;
    };
    template<class T> struct Holder final : HolderBase {
        explicit Holder(std::unique_ptr<T> value) : object(std::move(value)) {}
        bool present() const noexcept override { return bool(object); }
        std::unique_ptr<T> object;
    };
    UniqueInstance(std::type_index type, std::unique_ptr<HolderBase> holder)
        : m_type(type), m_holder(std::move(holder)) {}
    std::type_index m_type;
    std::unique_ptr<HolderBase> m_holder;
};

// 无全局状态，不提供同步保证；QObject 的创建线程由应用适配器负责。
class IInstanceProvider
{
public:
    virtual ~IInstanceProvider() = default;

    template<class T> std::shared_ptr<T> resolve(const QString &name = {})
    { return resolveOne<T>({typeid(T), name}); }

    template<class T> std::unique_ptr<T> create(const QString &name = {})
    {
        InstanceDetail::checkType<T>();
        const InstanceRequest request{typeid(T), name};
        return invoke(request, InstanceOperation::Create, [&] {
            auto result = createUnique(request);
            InstanceDetail::validate(result.m_type, result.m_holder && result.m_holder->present(),
                                     request, InstanceOperation::Create);
            // holder 仅由 from<T> 构造；验证契约身份后才恢复类型并移交所有权。
            return std::move(static_cast<UniqueInstance::Holder<T> *>(result.m_holder.get())->object);
        });
    }

    template<class T> std::vector<std::shared_ptr<T>> resolveAll()
    {
        InstanceDetail::checkType<T>();
        const InstanceRequest request{typeid(T), {}};
        return invoke(request, InstanceOperation::ResolveAll, [&] {
            auto results = resolveCollection(request);
            std::vector<std::shared_ptr<T>> objects;
            objects.reserve(results.size());
            for (auto &result : results) {
                InstanceDetail::validate(result.m_type, bool(result.m_object), request, InstanceOperation::ResolveAll);
                objects.push_back(std::static_pointer_cast<T>(std::move(result.m_object)));
            }
            return objects;
        });
    }

protected:
    // nullopt 仅表示当前请求未登记；后端依赖错误必须抛出，不转换为入口缺失。
    virtual std::optional<SharedInstance> resolveShared(const InstanceRequest &request) = 0;
    virtual UniqueInstance createUnique(const InstanceRequest &request);
    // 适配器显式提供集合；原样保留顺序和重复项，不扫描或合并单实例登记。
    virtual std::vector<SharedInstance> resolveCollection(const InstanceRequest &request);

private:
    template<class Action>
    static decltype(auto) invoke(const InstanceRequest &request, InstanceOperation operation, Action action)
    {
        try { return action(); }
        catch (const InstanceError &) { throw; }
        catch (const std::exception &error) {
            std::throw_with_nested(InstanceError(InstanceErrorCode::CreationFailed, operation, request,
                                                QString::fromUtf8(error.what())));
        }
        catch (...) {
            std::throw_with_nested(InstanceError(InstanceErrorCode::CreationFailed, operation, request,
                                                QStringLiteral("未知适配器异常")));
        }
    }

    template<class T> std::shared_ptr<T> resolveOne(const InstanceRequest &request)
    {
        InstanceDetail::checkType<T>();
        constexpr auto operation = InstanceOperation::Resolve;
        return invoke(request, operation, [&]() -> std::shared_ptr<T> {
            auto result = resolveShared(request);
            if (!result) {
                throw InstanceError(InstanceErrorCode::NotRegistered, operation, request,
                                    QStringLiteral("契约未登记"));
            }
            InstanceDetail::validate(result->m_type, bool(result->m_object), request, operation);
            return std::static_pointer_cast<T>(std::move(result->m_object));
        });
    }
};
