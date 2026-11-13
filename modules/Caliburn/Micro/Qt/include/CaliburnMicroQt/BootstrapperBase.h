#pragma once

#include <QObject>
#include <CaliburnMicroQt/WindowOptions.h>
#include <CaliburnMicroQt/IInstanceProvider.h>
#include <QHash>
#include <QVariant>
#include <functional>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

class QGuiApplication;
class IWindowManager;
class IWindowManagerLifecycle;
namespace IoCDetail { class Binding; }

// 根窗口角色固定为 Primary，其余设置委托窗口服务。
struct RootViewOptions { WindowOptions window; };

// 方法采用 Qt 风格 camelCase，与 Caliburn.Micro 保持概念对应。
// 运行单个根窗口；应用在 configure 中提供映射和对象工厂。
class BootstrapperBase : public QObject
{
public:
    explicit BootstrapperBase(QGuiApplication &app);
    ~BootstrapperBase() override;

    // 在应用主线程调用一次，返回事件循环退出码或失败码。
    int run();

protected:
    virtual bool configure() = 0;
    virtual void onStartup() {}
    // 启动中途失败也会调用，须允许应用资源尚未创建。
    virtual void onExit() {}

    // 仅配置阶段设置一次；登记不解析，引用保留至退出清理结束。
    [[nodiscard]] bool setInstanceProvider(std::shared_ptr<IInstanceProvider> provider);

    template<class T>
    [[nodiscard]] bool registerRootFactory(
            std::function<std::unique_ptr<T>(std::shared_ptr<IWindowManager>)> factory)
    {
        static_assert(std::is_base_of_v<QObject, T>, "根模型必须继承 QObject");
        if (m_state != State::Configuring)
            return fail(QStringLiteral("Root factories can only be registered during configure"));
        const auto *type = &T::staticMetaObject;
        if (m_factories.contains(type))
            return fail(QStringLiteral("Duplicate root factory registration"));

        if (!factory)
            return fail(QStringLiteral("Root factory must not be empty"));

        m_factories.insert(type, std::move(factory));
        return true;
    }

    template<class T>
    [[nodiscard]] bool displayRootViewFor(RootViewOptions options = {},
                                         const std::optional<QString> &context = std::nullopt)
    {
        static_assert(std::is_base_of_v<QObject, T>, "根模型必须继承 QObject");
        if (!createRootViewModel(&T::staticMetaObject, [this] {
                return std::unique_ptr<QObject>(m_instanceProvider->create<T>());
            })) {
            m_rootDisplayFailed = true;
            return false;
        }
        // 保留具体 T* 类型供 QML 注入；对象仍由 m_root 持有。
        const bool displayed = displayRootView(QVariant::fromValue(static_cast<T *>(m_root.get())), options, context);
        if (!displayed) m_rootDisplayFailed = true;
        return displayed;
    }

private:
    enum class State { Ready, Configuring, Starting, Running, Stopping, Stopped };
    using RootFactory = std::function<std::unique_ptr<QObject>(std::shared_ptr<IWindowManager>)>;

    bool initialize();
    bool selectWindowService(std::shared_ptr<IWindowManager> service);
    void disconnectWindowService() noexcept;
    void prepareWindows() noexcept;
    bool createRootViewModel(const QMetaObject *type,
                             const std::function<std::unique_ptr<QObject>()> &providerFactory);
    bool displayRootView(const QVariant &viewModel, RootViewOptions options, const std::optional<QString> &context);
    bool m_previousQuitOnLastWindowClosed = true;
    bool m_quitPolicyChanged = false;
    bool fail(const QString &message) const;
    void closeRoot() noexcept;
    void notifyExit() noexcept;
    void shutdown(bool invokeExit) noexcept;

    QGuiApplication &m_app;
    State m_state = State::Ready;
    QHash<const QMetaObject *, RootFactory> m_factories;
    std::unique_ptr<QObject> m_root;
    std::shared_ptr<IWindowManager> m_windowManager;
    IWindowManagerLifecycle *m_windowLifecycle = nullptr; // 借用同一服务的能力。
    std::shared_ptr<IInstanceProvider> m_instanceProvider;
    std::unique_ptr<IoCDetail::Binding> m_iocBinding;
    QMetaObject::Connection m_windowClosedConnection;
    QMetaObject::Connection m_windowCleanupConnection;
    bool m_displayAttempted = false;
    bool m_rootDisplayed = false;
    bool m_rootDisplayFailed = false;
    bool m_closeAttempted = false;
    bool m_cleanupFailed = false;
    bool m_exitNotified = false;
};
