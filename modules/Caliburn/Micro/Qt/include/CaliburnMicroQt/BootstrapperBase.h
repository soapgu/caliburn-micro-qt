#pragma once

#include <CaliburnMicroQt/ScreenViewModel.h>
#include <QHash>
#include <QVariant>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

class QGuiApplication;
class IWindowManager;
class WindowManager;

// 根窗口设置的扩展位置，目前不影响显示行为。
struct RootViewOptions {};

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

    template<class T>
    [[nodiscard]] bool registerRootFactory(
            std::function<std::unique_ptr<T>(std::shared_ptr<IWindowManager>)> factory)
    {
        static_assert(std::is_base_of_v<ScreenViewModel, T>, "根 VM 必须继承 ScreenViewModel");
        if (m_state != State::Configuring)
            return fail(QStringLiteral("根工厂只能在 configure 中登记"));
        const auto *type = &T::staticMetaObject;
        if (m_factories.contains(type))
            return fail(QStringLiteral("根工厂重复登记"));

        if (!factory)
            return fail(QStringLiteral("根工厂不能为空"));

        m_factories.insert(type, std::move(factory));
        return true;
    }

    template<class T>
    [[nodiscard]] bool displayRootViewFor(RootViewOptions options = {})
    {
        static_assert(std::is_base_of_v<ScreenViewModel, T>, "根 VM 必须继承 ScreenViewModel");
        Q_UNUSED(options);
        if (!createRootViewModel(&T::staticMetaObject)) {
            m_rootDisplayFailed = true;
            return false;
        }
        // 保留具体 T* 类型供 QML 注入；对象仍由 m_root 持有。
        const bool displayed = displayRootView(QVariant::fromValue(static_cast<T *>(m_root.get())));
        if (!displayed) m_rootDisplayFailed = true;
        return displayed;
    }

private:
    enum class State { Ready, Configuring, Starting, Running, Stopping, Stopped };
    using RootFactory = std::function<std::unique_ptr<ScreenViewModel>(std::shared_ptr<IWindowManager>)>;

    bool initialize();
    bool createRootViewModel(const QMetaObject *type);
    bool displayRootView(const QVariant &viewModel);
    bool fail(const QString &message) const;
    void closeRoot() noexcept;
    void notifyExit() noexcept;
    void shutdown(bool invokeExit) noexcept;

    QGuiApplication &m_app;
    State m_state = State::Ready;
    QHash<const QMetaObject *, RootFactory> m_factories;
    std::unique_ptr<ScreenViewModel> m_root;
    std::shared_ptr<WindowManager> m_windowManager;
    bool m_displayAttempted = false;
    bool m_rootDisplayed = false;
    bool m_rootDisplayFailed = false;
    bool m_closeAttempted = false;
    bool m_cleanupFailed = false;
    bool m_exitNotified = false;
};
