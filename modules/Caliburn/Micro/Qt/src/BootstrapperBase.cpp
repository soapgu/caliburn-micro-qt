#include "FrameworkLog.h"
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
        qCCritical(FrameworkLog::bootstrapper).noquote() << "Bootstrapper:" << stage << error.what();
    } catch (...) {
        qCCritical(FrameworkLog::bootstrapper).noquote() << "Bootstrapper:" << stage << "failed with an unknown exception";
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
        fail(QStringLiteral("Bootstrapper must be created and run on the application thread"));
        return 1;
    }
    if (m_state != State::Ready) {
        fail(QStringLiteral("Bootstrapper can only run once per instance"));
        return 1;
    }

    qCDebug(FrameworkLog::bootstrapper).noquote() << "Starting bootstrapper:" << FrameworkLog::identity(this);
    int exitCode = 1;
    try {
        if (initialize()) {
            m_state = State::Running;
            qCInfo(FrameworkLog::bootstrapper).noquote() << "Startup completed:" << FrameworkLog::identity(this);
            exitCode = m_app.exec();
        }
    } catch (const std::exception &error) {
        qCCritical(FrameworkLog::bootstrapper).noquote() << "Bootstrapper: startup or execution failed:" << error.what();
    } catch (...) {
        fail(QStringLiteral("Startup or execution failed with an unknown exception"));
    }
    // 此时派生类仍然完整；在事件循环返回后释放 QML 对象。
    shutdown(true);
    const int result = exitCode == 0 && m_cleanupFailed ? 1 : exitCode;
    qCInfo(FrameworkLog::bootstrapper) << "Bootstrapper exited; exit code:" << result;
    return result;
}

bool BootstrapperBase::initialize()
{
    qCDebug(FrameworkLog::bootstrapper) << "Configuring application";
    m_state = State::Configuring;
    m_windowManager = std::make_shared<WindowManager>();
    connect(m_windowManager.get(), &WindowManager::windowCleanupFailed, this, [this] { m_cleanupFailed = true; });
    QQmlEngine::setObjectOwnership(m_windowManager.get(), QQmlEngine::CppOwnership);
    if (!configure())
        return fail(QStringLiteral("configure failed"));
    if (!ViewRegistry::freeze())
        return fail(QStringLiteral("ViewRegistry freeze failed"));
    m_state = State::Starting;
    onStartup();
    if (!m_rootDisplayed || m_rootDisplayFailed)
        return fail(QStringLiteral("onStartup did not display the root window successfully"));
    return true;
}

bool BootstrapperBase::createRootViewModel(const QMetaObject *type)
{
    if (m_state != State::Starting)
        return fail(QStringLiteral("The root window can only be displayed during onStartup"));
    if (m_displayAttempted)
        return fail(QStringLiteral("Only one root window can be displayed"));
    m_displayAttempted = true;

    const auto found = m_factories.constFind(type);
    if (found == m_factories.cend())
        return fail(QStringLiteral("No factory registered for the root type"));
    auto model = (*found)(m_windowManager);
    if (!model)
        return fail(QStringLiteral("Root factory returned a null model"));
    if (model->parent() || model->thread() != m_app.thread())
        return fail(QStringLiteral("Root model must have no QObject parent and belong to the application thread"));

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
    if (!tryCleanup("Close root model", [this] { m_root->deactivate(true); }))
        m_cleanupFailed = true;
}

void BootstrapperBase::notifyExit() noexcept
{
    if (m_exitNotified) return;
    m_exitNotified = true;
    if (!tryCleanup("Execute onExit", [this] { onExit(); }))
        m_cleanupFailed = true;
}

void BootstrapperBase::shutdown(bool invokeExit) noexcept
{
    if (m_state == State::Stopping || m_state == State::Stopped)
        return;
    qCDebug(FrameworkLog::bootstrapper).noquote() << "Releasing application resources:" << FrameworkLog::identity(this);
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
    qCCritical(FrameworkLog::bootstrapper).noquote() << "Bootstrapper:" << message;
    return false;
}
