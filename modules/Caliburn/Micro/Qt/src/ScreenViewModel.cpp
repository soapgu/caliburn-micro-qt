#include <CaliburnMicroQt/ScreenViewModel.h>

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
    onInitialize();
    setAndNotify(m_initialized, true, &ScreenViewModel::isInitializedChanged);
}

void ScreenViewModel::activate()
{
    if (m_active)
        return;
    initialize();
    onActivate();
    setAndNotify(m_active, true, &ScreenViewModel::isActiveChanged);
}

void ScreenViewModel::deactivate(bool close)
{
    // 与 CM Screen 一致：活动对象可停用，已初始化对象可反复关闭。
    if (!(m_active || (m_initialized && close)))
        return;
    onDeactivate(close);
    setAndNotify(m_active, false, &ScreenViewModel::isActiveChanged);
}
