#include <CaliburnMicroQt/BootstrapperBase.h>
#include <CaliburnMicroQt/ViewRegistry.h>
#include <QDebug>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
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
        qCritical().noquote() << "Bootstrapper：" << stage << error.what();
    } catch (...) {
        qCritical().noquote() << "Bootstrapper：" << stage << "发生未知异常";
    }
    return false;
}
}

BootstrapperBase::BootstrapperBase(QGuiApplication &app) : m_app(app)
{
    connect(&m_app, &QCoreApplication::aboutToQuit, this, [this] { CloseRoot(); });
}

BootstrapperBase::~BootstrapperBase()
{
    // 正常清理由 Run 完成；基类析构不调用派生类退出钩子。
    Shutdown(false);
}

int BootstrapperBase::Run()
{
    if (QThread::currentThread() != m_app.thread() || thread() != m_app.thread()) {
        Fail(QStringLiteral("Bootstrapper 必须在应用主线程创建和运行"));
        return 1;
    }
    if (m_state != State::Ready) {
        Fail(QStringLiteral("同一实例只能运行一次"));
        return 1;
    }

    int exitCode = 1;
    try {
        if (Initialize()) {
            m_state = State::Running;
            exitCode = m_app.exec();
        }
    } catch (const std::exception &error) {
        qCritical().noquote() << "Bootstrapper：启动或运行失败：" << error.what();
    } catch (...) {
        Fail(QStringLiteral("启动或运行发生未知异常"));
    }
    // 此时派生类仍然完整；在事件循环返回后释放 QML 对象。
    Shutdown(true);
    return exitCode == 0 && m_cleanupFailed ? 1 : exitCode;
}

bool BootstrapperBase::Initialize()
{
    m_state = State::Configuring;
    if (!Configure())
        return Fail(QStringLiteral("Configure 失败"));
    if (!ViewRegistry::freeze())
        return Fail(QStringLiteral("ViewRegistry 冻结失败"));
    m_state = State::Starting;
    if (!OnStartup())
        return Fail(QStringLiteral("OnStartup 失败"));
    if (!m_rootDisplayed)
        return Fail(QStringLiteral("OnStartup 未成功显示根窗口"));
    return true;
}

bool BootstrapperBase::DisplayRootView(const QMetaObject *type)
{
    if (m_state != State::Starting)
        return Fail(QStringLiteral("只能在 OnStartup 中显示根窗口"));
    if (m_displayAttempted)
        return Fail(QStringLiteral("只允许显示一个根窗口"));
    m_displayAttempted = true;

    const auto found = m_factories.constFind(type);
    if (found == m_factories.cend())
        return Fail(QStringLiteral("未登记对应的根工厂"));
    auto instance = (*found)();
    if (!instance.model)
        return Fail(QStringLiteral("根工厂返回了空对象"));
    if (instance.model->parent() || instance.model->thread() != m_app.thread())
        return Fail(QStringLiteral("根 VM 必须无父对象且位于应用主线程"));

    m_root = std::move(instance.model);
    QQmlEngine::setObjectOwnership(m_root.get(), QQmlEngine::CppOwnership);
    const QUrl url = ViewRegistry::viewUrl(m_root.get());
    if (url.isEmpty())
        return Fail(QStringLiteral("未找到根 View 映射"));
    if (url.scheme() != QStringLiteral("qrc") && !url.isLocalFile())
        return Fail(QStringLiteral("根 View 必须使用本地或 qrc 地址"));

    m_root->initialize();
    m_root->activate();
    m_engine = std::make_unique<QQmlApplicationEngine>();
    m_engine->setInitialProperties({{QStringLiteral("viewModel"), instance.viewModel}});
    m_engine->load(url);
    const auto roots = m_engine->rootObjects();
    if (roots.size() != 1 || !qobject_cast<QQuickWindow *>(roots.first()))
        return Fail(QStringLiteral("根 View 加载失败或根对象不是窗口"));
    m_rootDisplayed = true;
    return true;
}

void BootstrapperBase::CloseRoot() noexcept
{
    if (!m_root || m_closeAttempted)
        return;
    m_closeAttempted = true;
    if (!tryCleanup("关闭根 VM", [this] { m_root->deactivate(true); }))
        m_cleanupFailed = true;
}

void BootstrapperBase::Shutdown(bool invokeExit) noexcept
{
    if (m_state == State::Stopping || m_state == State::Stopped)
        return;
    m_state = State::Stopping;
    CloseRoot();
    if (invokeExit && !tryCleanup("执行 OnExit", [this] { OnExit(); }))
        m_cleanupFailed = true;
    // View 先于其借用的 VM 销毁。
    m_engine.reset();
    m_root.reset();
    m_factories.clear();
    m_state = State::Stopped;
}

bool BootstrapperBase::Fail(const QString &message) const
{
    qCritical().noquote() << "Bootstrapper：" << message;
    return false;
}
