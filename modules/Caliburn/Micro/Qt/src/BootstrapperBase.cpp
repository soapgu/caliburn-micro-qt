#include <CaliburnMicroQt/BootstrapperBase.h>
#include <CaliburnMicroQt/ViewRegistry.h>
#include <QQmlEngine>
#include <CaliburnMicroQt/WindowManager.h>
#include <QDebug>
#include <QGuiApplication>
#include <QThread>
#include <exception>

namespace {
// 单步清理失败时继续释放其余资源，异常不跨越 Qt 信号边界。
template<class Action>
bool tryCleanup(const char *stage, Action action) noexcept
{
    try {
        action();
        return true;
    } catch (const std::exception &error) {
        qCritical().noquote() << "Bootstrapper：" << stage << error.what();
    } catch (...) {
        qCritical().noquote() << "Bootstrapper：" << stage << "发生未知异常";
    }
    return false;
}
}

BootstrapperBase::BootstrapperBase(QGuiApplication &app) : m_app(app)
{
    connect(&m_app, &QCoreApplication::aboutToQuit, this, [this] {
        if (m_windowManager) m_windowManager->prepareForShutdown();
        closeRoot();
        notifyExit();
    });
}

BootstrapperBase::~BootstrapperBase()
{
    // 正常清理由 run 完成；基类析构不调用派生类退出钩子。
    shutdown(false);
}

int BootstrapperBase::run()
{
    if (QThread::currentThread() != m_app.thread() || thread() != m_app.thread()) {
        fail(QStringLiteral("Bootstrapper 必须在应用主线程创建和运行"));
        return 1;
    }
    if (m_state != State::Ready) {
        fail(QStringLiteral("同一实例只能运行一次"));
        return 1;
    }

    int exitCode = 1;
    try {
        if (initialize()) {
            m_state = State::Running;
            exitCode = m_app.exec();
        }
    } catch (const std::exception &error) {
        qCritical().noquote() << "Bootstrapper：启动或运行失败：" << error.what();
    } catch (...) {
        fail(QStringLiteral("启动或运行发生未知异常"));
    }
    // 此时派生类仍然完整；在事件循环返回后释放 QML 对象。
    shutdown(true);
    return exitCode == 0 && m_cleanupFailed ? 1 : exitCode;
}

bool BootstrapperBase::initialize()
{
    m_state = State::Configuring;
    m_windowManager = std::make_shared<WindowManager>();
    connect(m_windowManager.get(), &WindowManager::windowCleanupFailed, this, [this] { m_cleanupFailed = true; });
    QQmlEngine::setObjectOwnership(m_windowManager.get(), QQmlEngine::CppOwnership);
    if (!configure())
        return fail(QStringLiteral("configure 失败"));
    if (!ViewRegistry::freeze())
        return fail(QStringLiteral("ViewRegistry 冻结失败"));
    m_state = State::Starting;
    onStartup();
    if (!m_rootDisplayed || m_rootDisplayFailed)
        return fail(QStringLiteral("onStartup 未成功显示根窗口"));
    return true;
}

bool BootstrapperBase::createRootViewModel(const QMetaObject *type)
{
    if (m_state != State::Starting)
        return fail(QStringLiteral("只能在 onStartup 中显示根窗口"));
    if (m_displayAttempted)
        return fail(QStringLiteral("只允许显示一个根窗口"));
    m_displayAttempted = true;

    const auto found = m_factories.constFind(type);
    if (found == m_factories.cend())
        return fail(QStringLiteral("未登记对应的根工厂"));
    auto model = (*found)(m_windowManager);
    if (!model)
        return fail(QStringLiteral("根工厂返回了空对象"));
    if (model->parent() || model->thread() != m_app.thread())
        return fail(QStringLiteral("根 VM 必须无父对象且位于应用主线程"));

    m_root = std::move(model);
    connect(m_root.get(), &ScreenViewModel::attemptingDeactivation, this, [this](bool close) {
        if (close) m_closeAttempted = true;
    });
    QQmlEngine::setObjectOwnership(m_root.get(), QQmlEngine::CppOwnership);
    return true;
}

bool BootstrapperBase::displayRootView(const QVariant &viewModel)
{
    m_rootDisplayed = m_windowManager->showWindow(viewModel);
    return m_rootDisplayed;
}

void BootstrapperBase::closeRoot() noexcept
{
    if (!m_root || m_closeAttempted)
        return;
    m_closeAttempted = true;
    if (!tryCleanup("关闭根 VM", [this] { m_root->deactivate(true); }))
        m_cleanupFailed = true;
}

void BootstrapperBase::notifyExit() noexcept
{
    if (m_exitNotified) return;
    m_exitNotified = true;
    if (!tryCleanup("执行 onExit", [this] { onExit(); }))
        m_cleanupFailed = true;
}

void BootstrapperBase::shutdown(bool invokeExit) noexcept
{
    if (m_state == State::Stopping || m_state == State::Stopped)
        return;
    m_state = State::Stopping;
    if (m_windowManager) m_windowManager->prepareForShutdown();
    closeRoot();
    if (invokeExit) notifyExit();
    // View 先于其借用的 VM 销毁。即使业务仍持有窗口服务，也显式释放窗口资源。
    if (m_windowManager) m_windowManager->releaseWindows();
    m_root.reset();
    m_factories.clear();
    m_windowManager.reset();
    m_state = State::Stopped;
}

bool BootstrapperBase::fail(const QString &message) const
{
    qCritical().noquote() << "Bootstrapper：" << message;
    return false;
}
