#include "FrameworkLog.h"
#include "ViewHostState.h"
#include "ViewModelLifetime.h"
#include <QThread>
#include <QVariant>

bool ViewHostState::matchesView(QObject *view) const
{
    return m_model && view && view->property("viewModel").value<QObject *>() == m_model.data();
}

void ViewHostState::setModel(QObject *model)
{
    if (QThread::currentThread() != thread() || (model && model->thread() != thread())) {
        qCWarning(FrameworkLog::view, "ViewHost: model and host must belong to the same thread");
        return;
    }
    if (ViewModelLifetime::isRetiring(model)) model = nullptr;
    if (m_model == model)
        return;
    QObject::disconnect(m_destroyed);
    QObject::disconnect(m_releaseRequested);
    m_model = model;
    m_loadUrl.clear();
    if (model) {
        // 只加入 Conductor 已建立的回收流程，不为根窗口或模态框模型创建登记。
        if (auto *lifetime = ViewModelLifetime::find(model)) {
            m_releaseRequested = connect(lifetime, &ViewModelLifetime::releaseRequested,
                                         this, [this] { setModel(nullptr); });
        }
        m_destroyed = connect(model, &QObject::destroyed, this, [this] {
            // destroyed 到达时 QPointer 已经清空，仍必须显式通知宿主卸载。
            m_model.clear();
            emit modelChanged();
        });
    }
    emit modelChanged();
}

void ViewHostState::retainView(QObject *view)
{
    if (m_model && view && view->thread() == thread()) {
        if (auto *lifetime = ViewModelLifetime::find(m_model))
            lifetime->retainView(view);
    }
}

void ViewHostState::reportLoadStarted(const QUrl &url)
{
    m_loadIdentity = FrameworkLog::identity(m_model.data());
    m_loadUrl = url;
    qCDebug(FrameworkLog::view).noquote() << "View loading:" << m_loadIdentity << "URL:" << url.toString();
}

void ViewHostState::reportLoadSucceeded(const QUrl &url) const
{
    qCInfo(FrameworkLog::view).noquote() << "View assembled:" << m_loadIdentity << "URL:" << url.toString();
}

void ViewHostState::reportLoadFailed(const QString &message) const
{
    qCWarning(FrameworkLog::view).noquote() << message << "model:" << FrameworkLog::identity(m_model.data())
                                        << "URL:" << m_loadUrl.toString();
}
