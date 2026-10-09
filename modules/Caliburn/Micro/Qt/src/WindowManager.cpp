#include <CaliburnMicroQt/WindowManager.h>
#include <CaliburnMicroQt/ViewRegistry.h>
#include "DialogHostState.h"
#include <QQmlEngine>
#include <stdexcept>

struct WindowManager::Request
{
    QPromise<DialogResult> promise;
    QPointer<ScreenViewModel> vm;
    QPointer<QObject> requester;
    QString id;
    DialogResult result;
    std::exception_ptr error;
    QMetaObject::Connection requesterDestroyed;
    QMetaObject::Connection vmDestroyed;
    bool closing = false;
};

WindowManager::WindowManager(QObject *parent) : IWindowManager(parent) {}

WindowManager::~WindowManager()
{
    m_destroying = true;
    if (m_request) {
        complete(std::nullopt);
        finish();
    }
}

bool WindowManager::busy() const { return bool(m_request) || m_finishing; }
ScreenViewModel *WindowManager::currentDialog() const
{
    return m_request && !m_request->closing ? m_request->vm.data() : nullptr;
}
QString WindowManager::requestId() const { return m_request ? m_request->id : QString(); }

bool WindowManager::attachHost(DialogHostState *host)
{
    if (m_host || m_destroying)
        return false;
    m_host = host;
    return true;
}

void WindowManager::detachHost(DialogHostState *host)
{
    if (m_host != host)
        return;
    m_host = nullptr;
    if (m_request) {
        const auto id = requestId();
        if (!m_request->closing) {
            m_request->closing = true;
            m_request->result = std::nullopt;
        }
        emit currentDialogChanged();
        // 宿主析构完成后再传播关闭生命周期，避免 View 仍在析构中。
        QMetaObject::invokeMethod(this, [this, id] { release(id); }, Qt::QueuedConnection);
    }
}

QFuture<DialogResult> WindowManager::showDialogAsync(
        std::unique_ptr<ScreenViewModel> vm, QObject *requester)
{
    auto request = std::make_unique<Request>();
    request->promise.start();
    auto future = request->promise.future();
    auto reject = [&](std::exception_ptr error) {
        request->promise.setException(error);
        request->promise.finish();
        return future;
    };
    if (vm && vm->thread() != thread()) {
        // 错误线程的候选同样被消费，但须在其所属线程回收。
        vm.release()->deleteLater();
        return reject(std::make_exception_ptr(std::invalid_argument("WindowManager：弹窗 VM 线程不匹配")));
    }
    if (!vm || !requester || vm->parent() || vm->parentViewModel() || vm->isActive()
            || vm->thread() != thread() || requester->thread() != thread())
        return reject(std::make_exception_ptr(std::invalid_argument("WindowManager：弹窗 VM 或请求者无效")));
    if (busy() || m_destroying)
        return reject(std::make_exception_ptr(std::runtime_error("WindowManager：已有弹窗请求")));
    if (!m_host || !m_host->available())
        return reject(std::make_exception_ptr(std::runtime_error("WindowManager：没有可用 DialogHost")));
    if (ViewRegistry::viewUrl(vm.get()).isEmpty())
        return reject(std::make_exception_ptr(std::runtime_error("WindowManager：弹窗 VM 缺少 View 映射")));
    request->id = QString::number(++m_nextId);
    request->requester = requester;
    request->vm = vm.get();
    vm->setParent(this);
    QQmlEngine::setObjectOwnership(vm.get(), QQmlEngine::CppOwnership);
    vm.release();
    m_request = std::move(request);
    m_request->requesterDestroyed = connect(requester, &QObject::destroyed, this, [this] {
        complete(std::nullopt);
    });
    m_request->vmDestroyed = connect(m_request->vm, &QObject::destroyed, this, [this] {
        complete(std::nullopt);
    });
    m_starting = true;
    emit busyChanged();
    try {
        if (m_request && m_request->vm && !m_request->closing)
            m_request->vm->activate();
    } catch (...) {
        complete(std::nullopt, std::current_exception());
    }
    m_starting = false;
    if (m_request && m_request->closing)
        finish();
    else if (m_request)
        emit currentDialogChanged();
    return future;
}

void WindowManager::closeDialog(ScreenViewModel *vm, DialogResult result)
{
    if (vm && m_request && m_request->vm == vm)
        complete(result);
}

void WindowManager::cancelDialogsFor(QObject *requester)
{
    if (requester && m_request && m_request->requester == requester)
        complete(std::nullopt);
}

void WindowManager::fail(const QString &id, const QString &message)
{
    if (m_request && id == requestId())
        complete(std::nullopt, std::make_exception_ptr(std::runtime_error(message.toStdString())));
}

void WindowManager::release(const QString &id)
{
    if (m_request && m_request->closing && id == requestId() && !m_starting)
        finish();
}

void WindowManager::complete(DialogResult result, std::exception_ptr error)
{
    if (!m_request || m_request->closing)
        return;
    m_request->closing = true;
    m_request->result = result;
    m_request->error = error;
    emit currentDialogChanged();
    if (m_starting)
        return;
    if (m_host) {
        auto *host = m_host.data();
        const auto id = requestId();
        emit host->hideRequested(id);
    } else {
        finish();
    }
}

void WindowManager::finish()
{
    if (!m_request || m_starting)
        return;
    m_finishing = true;
    auto request = std::move(m_request);
    disconnect(request->requesterDestroyed);
    disconnect(request->vmDestroyed);
    // 转换期间仍 busy，防止关闭钩子重新发起请求。
    if (request->vm) {
        try {
            request->vm->deactivate(true);
        } catch (...) {
            if (!request->error)
                request->error = std::current_exception();
        }
        if (request->vm)
            request->vm->deleteLater();
    }
    m_finishing = false;
    emit currentDialogChanged();
    emit busyChanged();
    if (request->error)
        request->promise.setException(request->error);
    else
        request->promise.addResult(request->result);
    request->promise.finish();
}
