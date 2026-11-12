#include "FrameworkLog.h"
#include <CaliburnMicroQt/BootstrapperBase.h>
#include <CaliburnMicroQt/ViewLocator.h>
#include <QQmlEngine>
#include <CaliburnMicroQt/WindowManager.h>
#include <CaliburnMicroQt/ScreenExtensions.h>
#include <QDebug>
#include <QGuiApplication>
#include <QQuickWindow>
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
        prepareWindows();
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
    m_previousQuitOnLastWindowClosed = m_app.quitOnLastWindowClosed();
    m_app.setQuitOnLastWindowClosed(false);
    m_quitPolicyChanged = true;
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
    if (!configure())
        return fail(QStringLiteral("configure failed"));
    std::shared_ptr<IWindowManager> service;
    if (m_instanceProvider)
        service = m_instanceProvider->resolve<IWindowManager>();
    else
        service = std::make_shared<WindowManager>();
    if (!selectWindowService(std::move(service)))
        return false;
    if (!ViewLocator::freeze())
        return fail(QStringLiteral("ViewLocator freeze failed"));
    m_state = State::Starting;
    onStartup();
    if (!m_rootDisplayed || m_rootDisplayFailed)
        return fail(QStringLiteral("onStartup did not display the root window successfully"));
    return true;
}

bool BootstrapperBase::setInstanceProvider(std::shared_ptr<IInstanceProvider> provider)
{
    if (m_state != State::Configuring)
        return fail(QStringLiteral("Instance provider can only be set during configure"));
    if (!provider)
        return fail(QStringLiteral("Instance provider must not be null"));
    if (m_instanceProvider)
        return fail(QStringLiteral("Instance provider can only be set once"));
    m_instanceProvider = std::move(provider);
    return true;
}

void BootstrapperBase::disconnectWindowService() noexcept
{
    disconnect(m_windowClosedConnection);
    disconnect(m_windowCleanupConnection);
    m_windowClosedConnection = {};
    m_windowCleanupConnection = {};
}

bool BootstrapperBase::selectWindowService(std::shared_ptr<IWindowManager> service)
{
    if (!service || service->thread() != m_app.thread())
        return fail(QStringLiteral("Window service must belong to the application thread"));
    auto *lifecycle = qobject_cast<IWindowManagerLifecycle *>(service.get());
    if (!lifecycle)
        return fail(QStringLiteral("Window service must implement IWindowManagerLifecycle"));
    m_windowManager = std::move(service);
    m_windowLifecycle = lifecycle;
    // 已取得清理能力的同线程候选即纳入失败清理，包括 parent／订阅验证失败。
    if (m_windowManager->parent())
        return fail(QStringLiteral("Window service must have no QObject parent"));
    m_windowCleanupConnection = m_windowLifecycle->subscribeWindowCleanupFailed(
            this, [this] { m_cleanupFailed = true; });
    if (!m_windowCleanupConnection)
        return fail(QStringLiteral("Window service cleanup subscription failed"));
    m_windowClosedConnection = connect(m_windowManager.get(), &IWindowManager::windowClosed,
            this, [this](QObject *model) {
        if (model && model == m_root.get()) {
            QMetaObject::invokeMethod(this, [this] {
                if (m_state == State::Running) m_app.quit();
            }, Qt::QueuedConnection);
        }
    });
    QQmlEngine::setObjectOwnership(m_windowManager.get(), QQmlEngine::CppOwnership);
    return true;
}

bool BootstrapperBase::createRootViewModel(const QMetaObject *type,
        const std::function<OwnedInstance<QObject>()> &providerFactory)
{
    if (m_state != State::Starting)
        return fail(QStringLiteral("The root window can only be displayed during onStartup"));
    if (m_displayAttempted)
        return fail(QStringLiteral("Only one root window can be displayed"));
    m_displayAttempted = true;

    const auto found = m_factories.constFind(type);
    OwnedInstance<QObject> model;
    if (found != m_factories.cend()) {
        auto legacy = (*found)(m_windowManager);
        std::function<void(QObject *)> deleter = [](QObject *object) { delete object; };
        model = OwnedInstance<QObject>(legacy.release(), std::move(deleter));
    } else if (m_instanceProvider) {
        model = providerFactory();
    } else {
        return fail(QStringLiteral("No factory registered for the root type"));
    }
    if (!model)
        return fail(QStringLiteral("Root factory returned a null model"));
    if (model->parent() || model->thread() != m_app.thread())
        return fail(QStringLiteral("Root model must have no QObject parent and belong to the application thread"));

    m_root = std::move(model);
    if (auto *deactivator = qobject_cast<IDeactivate *>(m_root.get())) {
        deactivator->subscribeAttemptingDeactivation(this, [this](bool close) {
            if (close) m_closeAttempted = true;
        });
    }
    QQmlEngine::setObjectOwnership(m_root.get(), QQmlEngine::CppOwnership);
    return true;
}

bool BootstrapperBase::displayRootView(const QVariant &viewModel, RootViewOptions options,
                                      const std::optional<QString> &context)
{
    options.window.role = WindowRole::Primary;
    m_rootDisplayed = m_windowManager->showWindow(viewModel, options.window, context);
    if (m_rootDisplayed) {
        if (auto *window = m_windowManager->windowFor(m_root.get())) {
            connect(window, &QObject::destroyed, this, [this] {
                if (m_state != State::Starting && m_state != State::Running) return;
                m_cleanupFailed = true;
                QMetaObject::invokeMethod(this, [this] {
                    if (m_state == State::Running) m_app.exit(1);
                }, Qt::QueuedConnection);
            });
        }
    }
    return m_rootDisplayed;
}

void BootstrapperBase::closeRoot() noexcept
{
    if (!m_root || m_closeAttempted)
        return;
    m_closeAttempted = true;
    if (!tryCleanup("Close root model", [this] { ScreenExtensions::tryDeactivate(m_root.get(), true); }))
        m_cleanupFailed = true;
}

void BootstrapperBase::notifyExit() noexcept
{
    if (m_exitNotified) return;
    m_exitNotified = true;
    if (!tryCleanup("Execute onExit", [this] { onExit(); }))
        m_cleanupFailed = true;
}

void BootstrapperBase::prepareWindows() noexcept
{
    if (m_windowLifecycle && !tryCleanup("Prepare window shutdown", [this] {
            m_windowLifecycle->prepareForShutdown();
        })) m_cleanupFailed = true;
}

void BootstrapperBase::shutdown(bool invokeExit) noexcept
{
    if (m_state == State::Stopping || m_state == State::Stopped)
        return;
    qCDebug(FrameworkLog::bootstrapper).noquote() << "Releasing application resources:" << FrameworkLog::identity(this);
    m_state = State::Stopping;
    prepareWindows();
    closeRoot();
    if (invokeExit) notifyExit();
    // View 先于其借用的 VM 销毁。即使业务仍持有窗口服务，也显式释放窗口资源。
    if (m_windowLifecycle && !tryCleanup("Release windows", [this] {
            m_windowLifecycle->releaseWindows();
        })) m_cleanupFailed = true;
    m_root.reset();
    m_factories.clear();
    disconnectWindowService();
    m_windowLifecycle = nullptr;
    m_windowManager.reset();
    m_instanceProvider.reset();
    if (m_quitPolicyChanged) {
        m_app.setQuitOnLastWindowClosed(m_previousQuitOnLastWindowClosed);
        m_quitPolicyChanged = false;
    }
    m_state = State::Stopped;
}

bool BootstrapperBase::fail(const QString &message) const
{
    qCCritical(FrameworkLog::bootstrapper).noquote() << "Bootstrapper:" << message;
    return false;
}
