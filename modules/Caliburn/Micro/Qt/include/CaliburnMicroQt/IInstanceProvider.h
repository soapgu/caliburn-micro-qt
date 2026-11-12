#pragma once

#include <QString>
#include <exception>
#include <functional>
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

enum class InstanceOperation { Resolve, TryResolve, Create, ResolveAll, BuildUp, Register };
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

// 删除器绑定此次创建的实际对象，不要求契约基类具有虚析构函数。
// 可移动、release 或 reset(nullptr)，不得 reset 为另一个对象或为无关指针复用删除器。
template<class T> using OwnedInstance = std::unique_ptr<T, std::function<void(T *)>>;

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

    // 仅消费独占对象；在释放输入之前完成可能抛异常的删除器构造。
    template<class Contract, class Implementation>
    static UniqueInstance from(std::unique_ptr<Implementation> object)
    {
        InstanceDetail::checkType<Contract>();
        static_assert(!std::is_array_v<Implementation>, "独占结果不支持数组");
        Contract *contract = object.get();
        auto *actual = object.get();
        std::function<void(void *)> deleter = [actual](void *) { delete actual; };
        UniqueInstance result(typeid(Contract), contract, std::move(deleter));
        object.release();
        return result;
    }
private:
    friend class IInstanceProvider;
    UniqueInstance(std::type_index type, void *object, std::function<void(void *)> deleter)
        : m_type(type), m_object(object, std::move(deleter)) {}
    std::type_index m_type;
    std::unique_ptr<void, std::function<void(void *)>> m_object;
};

// 同步借用，不允许适配器保留此包装或据此接管对象。
class BorrowedInstance
{
public:
    template<class T> T &as() const
    {
        InstanceDetail::checkType<T>();
        InstanceDetail::validate(m_type, true, {typeid(T), {}}, InstanceOperation::BuildUp);
        return *static_cast<T *>(m_object);
    }
private:
    friend class IInstanceProvider;
    template<class T> explicit BorrowedInstance(T &object) : m_type(typeid(T)), m_object(&object) {}
    std::type_index m_type;
    void *m_object;
};

// 无全局状态，不提供同步保证；QObject 的创建线程由应用适配器负责。
class IInstanceProvider
{
public:
    virtual ~IInstanceProvider() = default;

    template<class T> std::shared_ptr<T> resolve(const QString &name = {})
    { return resolveOne<T>({typeid(T), name}, InstanceOperation::Resolve, false); }

    template<class T> std::shared_ptr<T> tryResolve(const QString &name = {})
    { return resolveOne<T>({typeid(T), name}, InstanceOperation::TryResolve, true); }

    template<class T> OwnedInstance<T> create(const QString &name = {})
    {
        InstanceDetail::checkType<T>();
        const InstanceRequest request{typeid(T), name};
        return invoke(request, InstanceOperation::Create, [&] {
            auto result = createUnique(request);
            InstanceDetail::validate(result.m_type, bool(result.m_object), request, InstanceOperation::Create);
            // 先复制删除器，成功后才转移对象；分配异常时原结果仍负责回收。
            auto erasedDeleter = result.m_object.get_deleter();
            std::function<void(T *)> deleter = [erasedDeleter = std::move(erasedDeleter)](T *p) {
                erasedDeleter(p);
            };
            auto *pointer = static_cast<T *>(result.m_object.release());
            return OwnedInstance<T>(pointer, std::move(deleter));
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

    template<class T> void buildUp(T &object)
    {
        InstanceDetail::checkType<T>();
        const InstanceRequest request{typeid(T), {}};
        invoke(request, InstanceOperation::BuildUp, [&] { injectExisting(request, BorrowedInstance(object)); });
    }

protected:
    // nullopt 仅表示当前请求未登记；依赖缺失必须抛 InstanceError。
    virtual std::optional<SharedInstance> resolveShared(const InstanceRequest &request) = 0;
    virtual UniqueInstance createUnique(const InstanceRequest &request);
    // 包含命名与未命名项，按登记顺序返回，不按对象指针去重。
    virtual std::vector<SharedInstance> resolveCollection(const InstanceRequest &request);
    virtual void injectExisting(const InstanceRequest &request, BorrowedInstance object);

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

    template<class T> std::shared_ptr<T> resolveOne(const InstanceRequest &request,
                                                  InstanceOperation operation, bool optional)
    {
        InstanceDetail::checkType<T>();
        return invoke(request, operation, [&]() -> std::shared_ptr<T> {
            auto result = resolveShared(request);
            if (!result) {
                if (optional) return {};
                throw InstanceError(InstanceErrorCode::NotRegistered, operation, request,
                                    QStringLiteral("契约未登记"));
            }
            InstanceDetail::validate(result->m_type, bool(result->m_object), request, operation);
            return std::static_pointer_cast<T>(std::move(result->m_object));
        });
    }
};
