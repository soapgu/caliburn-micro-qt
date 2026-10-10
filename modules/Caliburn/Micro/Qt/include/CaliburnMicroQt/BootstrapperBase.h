#pragma once

#include <CaliburnMicroQt/ScreenViewModel.h>
#include <QHash>
#include <QVariant>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

class QGuiApplication;
class QQmlApplicationEngine;
class IWindowManager;
class WindowManager;
class WindowConductor;

// 根窗口设置的扩展位置，目前不影响显示行为。
struct RootViewOptions {};

// 运行单个根窗口；应用在 Configure 中提供映射和对象工厂。
class BootstrapperBase : public QObject
{
public:
    explicit BootstrapperBase(QGuiApplication &app);
    ~BootstrapperBase() override;

    // 在应用主线程调用一次，返回事件循环退出码或失败码。
    int Run();

protected:
    virtual bool Configure() = 0;
    virtual void OnStartup() {}
    // 启动中途失败也会调用，须允许应用资源尚未创建。
    virtual void OnExit() {}

    template<class T>
    [[nodiscard]] bool RegisterRootFactory(
            std::function<std::unique_ptr<T>(std::shared_ptr<IWindowManager>)> factory)
    {
        static_assert(std::is_base_of_v<ScreenViewModel, T>, "根 VM 必须继承 ScreenViewModel");
        if (m_state != State::Configuring)
            return Fail(QStringLiteral("根工厂只能在 Configure 中登记"));
        const auto *type = &T::staticMetaObject;
        if (m_factories.contains(type))
            return Fail(QStringLiteral("根工厂重复登记"));

        if (!factory)
            return Fail(QStringLiteral("根工厂不能为空"));

        m_factories.insert(type, std::move(factory));
        return true;
    }

    template<class T>
    [[nodiscard]] bool DisplayRootViewFor(RootViewOptions options = {})
    {
        static_assert(std::is_base_of_v<ScreenViewModel, T>, "根 VM 必须继承 ScreenViewModel");
        Q_UNUSED(options);
        if (!CreateRootViewModel(&T::staticMetaObject)) {
            m_rootDisplayFailed = true;
            return false;
        }
        // 保留具体 T* 类型供 QML 注入；对象仍由 m_root 持有。
        const bool displayed = DisplayRootView(QVariant::fromValue(static_cast<T *>(m_root.get())));
        if (!displayed) m_rootDisplayFailed = true;
        return displayed;
    }

private:
    enum class State { Ready, Configuring, Starting, Running, Stopping, Stopped };
    using RootFactory = std::function<std::unique_ptr<ScreenViewModel>(std::shared_ptr<IWindowManager>)>;

    bool Initialize();
    bool CreateRootViewModel(const QMetaObject *type);
    bool DisplayRootView(const QVariant &viewModel);
    bool Fail(const QString &message) const;
    void CloseRoot() noexcept;
    void NotifyExit() noexcept;
    void Shutdown(bool invokeExit) noexcept;

    QGuiApplication &m_app;
    State m_state = State::Ready;
    QHash<const QMetaObject *, RootFactory> m_factories;
    std::unique_ptr<ScreenViewModel> m_root;
    std::unique_ptr<QQmlApplicationEngine> m_engine;
    std::shared_ptr<WindowManager> m_windowManager;
    std::unique_ptr<WindowConductor> m_windowConductor;
    bool m_displayAttempted = false;
    bool m_rootDisplayed = false;
    bool m_rootDisplayFailed = false;
    bool m_closeAttempted = false;
    bool m_cleanupFailed = false;
    bool m_exitNotified = false;
};
