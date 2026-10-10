#include <CaliburnMicroQt/ScreenViewModel.h>
#include <CaliburnMicroQt/IConductor.h>

ScreenViewModel::ScreenViewModel(QObject *parent) : ViewModelBase(parent) {}

void ScreenViewModel::setParentViewModel(QObject *parent)
{
    if (m_parentViewModel == parent)
        return;
    m_parentViewModel = parent;
    emit parentViewModelChanged();
}

void ScreenViewModel::initialize()
{
    if (m_initialized)
        return;
    setAndNotify(m_initialized, true, &ScreenViewModel::isInitializedChanged);
    onInitialize();
}

void ScreenViewModel::activate()
{
    if (m_active)
        return;
    initialize();
    setAndNotify(m_active, true, &ScreenViewModel::isActiveChanged);
    onActivate();
}

void ScreenViewModel::deactivate(bool close)
{
    // 与 CM Screen 一致：活动对象可停用，已初始化对象可反复关闭。
    if (!(m_active || (m_initialized && close)))
        return;
    emit attemptingDeactivation(close);
    setAndNotify(m_active, false, &ScreenViewModel::isActiveChanged);
    onDeactivate(close);
    emit deactivated(close);
}

void ScreenViewModel::tryClose()
{
    auto *conductor = qobject_cast<IConductor *>(parentViewModel());
    if (conductor) conductor->deactivateItem(this, true);
    else if (!parentViewModel()) emit closeRequested();
}

void ScreenViewModel::canClose(CloseCallback callback)
{
    callback(true);
}
