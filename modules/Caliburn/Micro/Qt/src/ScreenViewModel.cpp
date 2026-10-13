#include <CaliburnMicroQt/ScreenViewModel.h>
#include <CaliburnMicroQt/IConductor.h>
#include "FrameworkLog.h"
#include <utility>

ScreenViewModel::ScreenViewModel(QObject *parent) : ViewModelBase(parent) {}

void ScreenViewModel::setParentViewModel(QObject *parent)
{
    if (m_parentViewModel == parent)
        return;
    qCDebug(FrameworkLog::lifecycle).noquote() << "Logical parent changed:" << FrameworkLog::identity(this)
                                             << "parent:" << FrameworkLog::identity(parent);
    m_parentViewModel = parent;
    emit parentViewModelChanged();
}

void ScreenViewModel::initialize()
{
    if (m_initialized) {
        qCDebug(FrameworkLog::lifecycle).noquote() << "Initialize skipped:" << FrameworkLog::identity(this);
        return;
    }
    qCDebug(FrameworkLog::lifecycle).noquote() << "Initializing:" << FrameworkLog::identity(this);
    setAndNotify(m_initialized, true, &ScreenViewModel::isInitializedChanged);
    onInitialize();
}

void ScreenViewModel::activate()
{
    if (m_active) {
        qCDebug(FrameworkLog::lifecycle).noquote() << "Activate skipped:" << FrameworkLog::identity(this);
        return;
    }
    initialize();
    setAndNotify(m_active, true, &ScreenViewModel::isActiveChanged);
    qCInfo(FrameworkLog::lifecycle).noquote() << "Activating:" << FrameworkLog::identity(this);
    onActivate();
}

void ScreenViewModel::deactivate(bool close)
{
    // 与 CM Screen 一致：活动对象可停用，已初始化对象可反复关闭。
    if (!(m_active || (m_initialized && close))) {
        qCDebug(FrameworkLog::lifecycle).noquote() << "Deactivate skipped:" << FrameworkLog::identity(this)
                                                 << "close:" << close;
        return;
    }
    const auto identity = FrameworkLog::identity(this);
    emit attemptingDeactivation(close);
    setAndNotify(m_active, false, &ScreenViewModel::isActiveChanged);
    qCInfo(FrameworkLog::lifecycle).noquote() << "Deactivating:" << identity << "close:" << close;
    onDeactivate(close);
    emit deactivated(close);
    if (close) qCInfo(FrameworkLog::lifecycle).noquote() << "Closed:" << identity;
}

void ScreenViewModel::tryClose(DialogResult dialogResult)
{
    qCDebug(FrameworkLog::lifecycle).noquote() << "Close requested:" << FrameworkLog::identity(this);
    auto *conductor = qobject_cast<IConductor *>(parentViewModel());
    if (conductor) conductor->deactivateItem(this, true);
    else if (!parentViewModel()) emit closeRequested(dialogResult);
}

void ScreenViewModel::canClose(CloseCallback callback)
{
    callback(true);
}

QMetaObject::Connection ScreenViewModel::subscribeAttemptingDeactivation(
        QObject *context, DeactivationCallback callback)
{
    return connect(this, &ScreenViewModel::attemptingDeactivation, context, std::move(callback));
}

QMetaObject::Connection ScreenViewModel::subscribeDeactivated(
        QObject *context, DeactivationCallback callback)
{
    return connect(this, &ScreenViewModel::deactivated, context, std::move(callback));
}
