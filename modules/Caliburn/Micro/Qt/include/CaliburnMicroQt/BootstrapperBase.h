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
    virtual bool OnStartup() = 0;
    // 启动中途失败也会调用，须允许应用资源尚未创建。
    virtual void OnExit() {}

    template<class T, class Factory>
    [[nodiscard]] bool RegisterRootFactory(Factory factory)
    {
        static_assert(std::is_base_of_v<ScreenViewModel, T>, "根 VM 必须继承 ScreenViewModel");
        if (m_state != State::Configuring)
            return Fail(QStringLiteral("根工厂只能在 Configure 中登记"));
        const auto *type = &T::staticMetaObject;
        if (m_factories.contains(type))
            return Fail(QStringLiteral("根工厂重复登记"));

        std::function<std::unique_ptr<T>()> typedFactory = std::move(factory);
        if (!typedFactory)
            return Fail(QStringLiteral("根工厂不能为空"));

        auto wrappedFactory = [factory = std::move(typedFactory)]() -> RootInstance {
            auto model = factory();
            if (!model)
                return {};
            RootInstance result;
            // 普通指针保留 T* 类型供 QML 访问，不接管对象。
            result.viewModel = QVariant::fromValue(model.get());
            // 智能指针交接删除责任，统一通过 Screen 管理生命周期。
            result.model = std::move(model);
            return result;
        };
        m_factories.insert(type, std::move(wrappedFactory));
        return true;
    }

    template<class T>
    [[nodiscard]] bool DisplayRootViewFor()
    {
        static_assert(std::is_base_of_v<ScreenViewModel, T>, "根 VM 必须继承 ScreenViewModel");
        return DisplayRootView(&T::staticMetaObject);
    }

private:
    enum class State { Ready, Configuring, Starting, Running, Stopping, Stopped };
    struct RootInstance {
        std::unique_ptr<ScreenViewModel> model;
        QVariant viewModel;
    };
    using RootFactory = std::function<RootInstance()>;

    bool Initialize();
    bool DisplayRootView(const QMetaObject *type);
    bool Fail(const QString &message) const;
    void CloseRoot() noexcept;
    void Shutdown(bool invokeExit) noexcept;

    QGuiApplication &m_app;
    State m_state = State::Ready;
    QHash<const QMetaObject *, RootFactory> m_factories;
    std::unique_ptr<ScreenViewModel> m_root;
    std::unique_ptr<QQmlApplicationEngine> m_engine;
    bool m_displayAttempted = false;
    bool m_rootDisplayed = false;
    bool m_closeAttempted = false;
    bool m_cleanupFailed = false;
};
