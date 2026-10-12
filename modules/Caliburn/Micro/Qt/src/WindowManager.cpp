#include "FrameworkLog.h"
#include <CaliburnMicroQt/WindowManager.h>
#include <CaliburnMicroQt/ViewRegistry.h>
#include "WindowConductor.h"
#include <QQmlEngine>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScreen>
#include <QThread>
#include <algorithm>
#include <stdexcept>

namespace {
constexpr auto ownerProperty = "_caliburnWindowManager";

bool failWindow(const QString &message)
{
    qCWarning(FrameworkLog::window).noquote() << "WindowManager:" << message;
    return false;
}
}

// 仅拥有普通窗口资源；VM 由调用方持有，避免与业务的共享窗口服务形成所有权环。
struct WindowManager::ManagedWindow
{
    QPointer<ScreenViewModel> model;
    QPointer<QQuickWindow> window;
    bool closeAttempted = false;
    QMetaObject::Connection closeConnection;
    std::unique_ptr<QQmlApplicationEngine> engine;
    std::unique_ptr<WindowConductor> bridge;
};

bool WindowManager::showWindow(const QVariant &viewModel)
{
    if (QThread::currentThread() != thread())
        return failWindow(QStringLiteral("Showing a window requires the service thread"));
    auto *model = qobject_cast<ScreenViewModel *>(viewModel.value<QObject *>());
    if (!model || model->thread() != thread() || model->parentViewModel())
        return failWindow(QStringLiteral("Window model must be a Screen on the service thread without a logical parent"));
    if (m_managedWindow || m_window || busy() || m_destroying)
        return failWindow(QStringLiteral("A managed window, owner window or pending request already exists"));
    qCDebug(FrameworkLog::window).noquote() << "Showing window; model:" << FrameworkLog::identity(model);
    const QUrl url = ViewRegistry::viewUrl(model);
    if (url.isEmpty())
        return failWindow(QStringLiteral("No view mapping found for the window model"));
    if (url.scheme() != QStringLiteral("qrc") && !url.isLocalFile())
        return failWindow(QStringLiteral("Window view must use a local or qrc URL"));

    m_managedWindow = std::make_unique<ManagedWindow>();
    auto &managed = *m_managedWindow;
    managed.model = model;
    managed.closeConnection = connect(model, &ScreenViewModel::attemptingDeactivation,
            this, [this](bool close) {
        if (close && m_managedWindow) m_managedWindow->closeAttempted = true;
    });
    QQmlEngine::setObjectOwnership(model, QQmlEngine::CppOwnership);
    auto fail = [this](const QString &message) {
        releaseWindows();
        return failWindow(message);
    };
    try {
        // 保留当前先激活再加载的契约；CM 的创建/绑定/激活顺序单独迁移。
        model->activate();
        managed.engine = std::make_unique<QQmlApplicationEngine>();
        managed.engine->setInitialProperties({{QStringLiteral("viewModel"), viewModel}});
        managed.engine->load(url);
        const auto roots = managed.engine->rootObjects();
        auto *window = roots.size() == 1 ? qobject_cast<QQuickWindow *>(roots.first()) : nullptr;
        if (!window)
            return fail(QStringLiteral("Window view failed to load or its root is not a window: %1").arg(url.toString()));
        managed.window = window;
        if (!attachToWindow(window))
            return fail(QStringLiteral("Failed to attach the managed window"));
        managed.bridge = std::make_unique<WindowConductor>(window, model);
        connect(managed.bridge.get(), &WindowConductor::windowClosed,
                this, &WindowManager::closeManagedWindow);
        // 等当前对象及子 View 的析构栈退出再释放引擎；旧桥接销毁会取消旧清理。
        auto releaseLater = [this] {
            QMetaObject::invokeMethod(m_managedWindow->bridge.get(),
                    [this] { releaseWindows(); }, Qt::QueuedConnection);
        };
        connect(model, &QObject::destroyed, managed.bridge.get(), releaseLater);
        connect(window, &QObject::destroyed, managed.bridge.get(), releaseLater);
        const auto windowIdentity = FrameworkLog::identity(window);
        const auto modelIdentity = FrameworkLog::identity(model);
        window->show();
        qCInfo(FrameworkLog::window).noquote() << "Window shown:" << windowIdentity
                                             << "model:" << modelIdentity << "URL:" << url.toString();
        return true;
    } catch (...) {
        releaseWindows();
        throw;
    }
}

void WindowManager::closeManagedWindow() noexcept
{
    if (!m_managedWindow || !m_managedWindow->model || m_managedWindow->closeAttempted) return;
    m_managedWindow->closeAttempted = true;
    try { m_managedWindow->model->deactivate(true); }
    catch (const std::exception &error) {
        qCCritical(FrameworkLog::window).noquote() << "WindowManager: model close failed:" << error.what();
        emit windowCleanupFailed();
    } catch (...) {
        qCCritical(FrameworkLog::window, "WindowManager: model close failed with an unknown exception");
        emit windowCleanupFailed();
    }
}

void WindowManager::prepareForShutdown()
{
    if (QThread::currentThread() != thread()) {
        failWindow(QStringLiteral("Shutdown preparation requires the service thread"));
        return;
    }
    if (m_managedWindow && m_managedWindow->bridge) m_managedWindow->bridge->detach();
    detachFromWindow();
}

void WindowManager::releaseWindows()
{
    if (QThread::currentThread() != thread()) {
        failWindow(QStringLiteral("Window release requires the service thread"));
        return;
    }
    prepareForShutdown();
    if (!m_managedWindow) return;
    qCDebug(FrameworkLog::window).noquote() << "Releasing managed window resources; model:"
                                          << FrameworkLog::identity(m_managedWindow->model.data());
    m_managedWindow->bridge.reset();
    m_managedWindow->engine.reset();
    // 强制释放时 View 先于 VM 收尾；Bootstrapper 已关闭过的根不会重复关闭。
    closeManagedWindow();
    disconnect(m_managedWindow->closeConnection);
    m_managedWindow.reset();
}

struct WindowManager::Request
{
    QPromise<DialogResult> promise;
    QPointer<ScreenViewModel> vm;
    QPointer<QObject> requester;
    QPointer<QQuickWindow> window;
    QPointer<QQuickItem> previousFocus;
    std::unique_ptr<WindowConductor> bridge;
    QList<QMetaObject::Connection> connections;
    DialogResult result;
    std::exception_ptr error;
    bool closing = false;
    bool closePending = false;
    bool lifecycleAttempted = false;
    QString modelIdentity;
};

WindowManager::WindowManager(QObject *parent) : IWindowManager(parent) {}

WindowManager::~WindowManager()
{
    m_destroying = true;
    releaseWindows();
}

bool WindowManager::attachToWindow(QQuickWindow *window, QQuickItem *fallbackFocusItem)
{
    if (QThread::currentThread() != thread() || !window || window->thread() != thread()
            || (fallbackFocusItem && fallbackFocusItem->thread() != thread()) || m_destroying) {
        qCWarning(FrameworkLog::window, "WindowManager: invalid owner window or thread");
        return false;
    }
    if (m_window == window) return true;
    if (m_managedWindow && m_managedWindow->window && m_managedWindow->window != window)
        return failWindow(QStringLiteral("Release the managed window before attaching another owner window"));
    if (m_window || busy()) {
        qCWarning(FrameworkLog::window, "WindowManager: detach the current owner window before attaching another one");
        return false;
    }
    auto *engine = qmlEngine(window);
    if (!engine || engine->thread() != thread()) {
        qCWarning(FrameworkLog::window, "WindowManager: window has no usable QML engine");
        return false;
    }
    if (fallbackFocusItem && fallbackFocusItem->window() != window) {
        qCWarning(FrameworkLog::window, "WindowManager: fallback focus item must belong to the owner window");
        return false;
    }
    if (window->property(ownerProperty).value<QObject *>()) {
        qCWarning(FrameworkLog::window, "WindowManager: target window already has a window service");
        return false;
    }
    window->setProperty(ownerProperty, QVariant::fromValue(static_cast<QObject *>(this)));
    qCDebug(FrameworkLog::window).noquote() << "Owner window attached:" << FrameworkLog::identity(window);
    m_window = window;
    m_engine = engine;
    m_fallbackFocus = fallbackFocusItem ? fallbackFocusItem : window->contentItem();
    m_engineDestroyed = connect(engine, &QObject::destroyed, this, [this] { detachFromWindow(); });
    m_windowDestroyed = connect(window, &QObject::destroyed, this, [this] { detachFromWindow(); });
    return true;
}

void WindowManager::detachFromWindow()
{
    if (QThread::currentThread() != thread()) {
        qCWarning(FrameworkLog::window, "WindowManager: detaching requires the service thread");
        return;
    }
    if (m_window) qCDebug(FrameworkLog::window).noquote() << "Owner window detached:" << FrameworkLog::identity(m_window.data());
    disconnect(m_engineDestroyed);
    disconnect(m_windowDestroyed);
    if (m_window && m_window->property(ownerProperty).value<QObject *>() == this)
        m_window->setProperty(ownerProperty, QVariant());
    m_window = nullptr;
    m_engine = nullptr;
    m_fallbackFocus = nullptr;
    complete(std::nullopt);
}

bool WindowManager::busy() const { return bool(m_request) || m_finishing; }
ScreenViewModel *WindowManager::currentDialog() const
{
    return m_request && !m_request->closing ? m_request->vm.data() : nullptr;
}

QFuture<DialogResult> WindowManager::showDialogAsync(
        std::unique_ptr<ScreenViewModel> vm, QObject *requester)
{
    qCDebug(FrameworkLog::window) << "Dialog requested";
    auto request = std::make_unique<Request>();
    request->promise.start();
    auto future = request->promise.future();
    auto reject = [&](std::exception_ptr error) {
        qCWarning(FrameworkLog::window).noquote() << "Dialog request rejected:" << FrameworkLog::exceptionMessage(error);
        request->promise.setException(error);
        request->promise.finish();
        return future;
    };
    if (vm && vm->thread() != thread()) {
        vm.release()->deleteLater();
        return reject(std::make_exception_ptr(std::invalid_argument("WindowManager: dialog model belongs to another thread")));
    }
    if (!vm || !requester || vm->parent() || vm->parentViewModel() || vm->isActive()
            || vm->thread() != thread() || requester->thread() != thread())
        return reject(std::make_exception_ptr(std::invalid_argument("WindowManager: invalid dialog model or requester")));
    if (busy() || m_destroying)
        return reject(std::make_exception_ptr(std::runtime_error("WindowManager: a dialog request is already pending")));
    if (!m_window || !m_engine)
        return reject(std::make_exception_ptr(std::runtime_error("WindowManager: no usable owner window")));
    if (ViewRegistry::viewUrl(vm.get()).isEmpty())
        return reject(std::make_exception_ptr(std::runtime_error("WindowManager: dialog model has no view mapping")));
    request->modelIdentity = FrameworkLog::identity(vm.get());
    request->requester = requester;
    request->vm = vm.get();
    request->previousFocus = m_window->activeFocusItem();
    vm->setParent(this);
    QQmlEngine::setObjectOwnership(vm.get(), QQmlEngine::CppOwnership);
    vm.release();
    m_request = std::move(request);
    m_request->connections << connect(requester, &QObject::destroyed, this, [this] {
        complete(std::nullopt);
    }) << connect(m_request->vm, &QObject::destroyed, this, [this] {
        // 先让 QML 和 ViewHost 完成销毁通知，避免在 VM 析构栈中重入 Loader。
        m_request->closing = true;
        if (!m_starting && m_request->window)
            QMetaObject::invokeMethod(m_request->window, [this] { complete(std::nullopt); },
                                      Qt::QueuedConnection);
    }) << connect(m_request->vm, &ScreenViewModel::attemptingDeactivation, this, [this](bool close) {
        if (close && m_request) m_request->lifecycleAttempted = true;
    }) << connect(m_request->vm, &ScreenViewModel::closeRequested, this, [this](DialogResult result) {
        if (!m_request || m_request->closing || m_request->closePending
                || (m_request->bridge && m_request->bridge->isClosing())) return;
        // 在激活及桥接连接前保存结果，桥接收到同一信号时只负责请求关窗。
        m_request->result = result;
        m_request->closePending = true;
    });
    m_starting = true;
    emit busyChanged();
    try {
        if (!m_request->closing) m_request->vm->activate();
        if (!m_request->closing) {
            QQmlComponent component(m_engine, QUrl(QStringLiteral(
                "qrc:/qt/qml/Caliburn/Micro/Qt/DialogWindow.qml")));
            std::unique_ptr<QObject> object(component.createWithInitialProperties({
                {QStringLiteral("model"), QVariant::fromValue(m_request->vm.data())}
            }));
            auto *window = qobject_cast<QQuickWindow *>(object.get());
            if (!window)
                throw std::runtime_error(component.errorString().toStdString());
            const auto error = window->property("errorString").toString();
            if (!error.isEmpty()) throw std::runtime_error(error.toStdString());
            QQmlEngine::setObjectOwnership(window, QQmlEngine::CppOwnership);
            window->QObject::setParent(this);
            m_request->window = window;
            object.release();
            window->setTransientParent(m_window);
            window->setModality(Qt::ApplicationModal);
            window->setScreen(m_window->screen());
            const auto area = window->screen()->availableGeometry();
            window->resize(std::clamp(window->property("preferredWidth").toInt(), 1, area.width()),
                           std::clamp(window->property("preferredHeight").toInt(), 1, area.height()));
            const auto center = m_window->geometry().center();
            window->setPosition(std::clamp(center.x() - window->width()/2, area.left(),
                                          area.right() - window->width() + 1),
                                std::clamp(center.y() - window->height()/2, area.top(),
                                          area.bottom() - window->height() + 1));
            m_request->bridge = std::make_unique<WindowConductor>(window, m_request->vm);
            QPointer<QQuickWindow> dialog = window;
            m_request->connections << connect(m_request->bridge.get(), &WindowConductor::windowClosed,
                    this, [this, dialog] {
                if (dialog && m_request && m_request->window == dialog) finish(true);
            }, Qt::QueuedConnection);
            m_request->connections << connect(m_request->bridge.get(), &WindowConductor::closeRejected,
                    this, [this] {
                if (m_request) {
                    m_request->result = std::nullopt;
                    m_request->closePending = false;
                }
            });
            m_request->connections << connect(window, &QObject::destroyed, this, [this] {
                // QObject::destroyed 早于子对象释放，等窗口析构完成后再关闭 VM。
                m_request->closing = true;
                QMetaObject::invokeMethod(m_request->bridge.get(),
                        [this] { complete(std::nullopt); }, Qt::QueuedConnection);
            });
            // QML 的 Escape 与按钮、标题栏走同一个关闭桥接。
            connect(window, SIGNAL(dismissRequested()), m_request->bridge.get(), SLOT(requestClose()));
            window->show();
            window->requestActivate();
            if (auto *view = window->property("dialogItem").value<QQuickItem *>())
                view->forceActiveFocus(Qt::PopupFocusReason);
            qCInfo(FrameworkLog::window).noquote() << "Dialog shown:" << FrameworkLog::identity(window)
                                                 << "model:" << m_request->modelIdentity;
            emit currentDialogChanged();
            if (m_request->closePending) m_request->bridge->requestClose();
        }
    } catch (...) {
        complete(std::nullopt, std::current_exception());
    }
    m_starting = false;
    if (m_request && m_request->closing) finish();
    return future;
}

void WindowManager::complete(DialogResult result, std::exception_ptr error)
{
    if (!m_request) return;
    m_request->closing = true;
    m_request->result = result;
    if (error && !m_request->error) m_request->error = error;
    if (!m_starting) finish();
}

void WindowManager::finish(bool restoreFocus)
{
    if (!m_request || m_starting) return;
    m_finishing = true;
    auto request = std::move(m_request);
    for (const auto &connection : request->connections) disconnect(connection);
    request->bridge.reset();
    // 先卸载并销毁借用 VM 的 View；强制清理无需等待窗口或 QML 接受关闭。
    // 窗口父树同步释放 View/Loader，不在引擎析构期间执行 QML 函数。
    delete request->window.data();
    if (request->vm) {
        if (!request->lifecycleAttempted) {
            try { request->vm->deactivate(true); }
            catch (...) { if (!request->error) request->error = std::current_exception(); }
        }
        if (request->vm) request->vm->deleteLater();
    }
    if (restoreFocus && !m_destroying) {
        const auto previous = request->previousFocus;
        QMetaObject::invokeMethod(this, [this, previous] {
            if (busy() || !m_window || !m_window->isVisible()) return;
            m_window->requestActivate();
            auto *focus = previous && previous->window() == m_window && previous->isVisible()
                    && previous->isEnabled() ? previous.data() : m_fallbackFocus.data();
            if (focus && focus->isVisible() && focus->isEnabled()) {
                qCDebug(FrameworkLog::window).noquote() << "Restoring focus:" << FrameworkLog::identity(focus);
                focus->forceActiveFocus(Qt::PopupFocusReason);
            }
        }, Qt::QueuedConnection);
    }
    if (request->error)
        qCWarning(FrameworkLog::window).noquote() << "Dialog failed:" << request->modelIdentity
                                               << "reason:" << FrameworkLog::exceptionMessage(request->error);
    else
        qCInfo(FrameworkLog::window).noquote() << "Dialog completed:" << request->modelIdentity
                                            << "result:" << (request->result ? (*request->result ? "accepted" : "rejected") : "cancelled");
    m_finishing = false;
    emit currentDialogChanged();
    emit busyChanged();
    if (request->error) request->promise.setException(request->error);
    else request->promise.addResult(request->result);
    request->promise.finish();
}
