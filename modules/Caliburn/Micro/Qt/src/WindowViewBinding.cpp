#include "WindowViewBinding.h"
#include <CaliburnMicroQt/ScreenViewModel.h>
#include <stdexcept>

WindowViewBinding::WindowViewBinding(QObject *model, QObject *window, bool isDialog)
    : m_model(qobject_cast<ScreenViewModel *>(model))
{
    if (!window || !model || window->thread() != model->thread())
        throw std::invalid_argument("WindowViewBinding: invalid model, window or thread");
    if (!canBind(model))
        throw std::invalid_argument("WindowViewBinding: model already has an associated window");
    if (m_model) {
        m_model->m_windowView = window;
        m_model->m_windowBinding = this;
        m_model->m_windowIsDialog = isDialog;
    }
}

WindowViewBinding::~WindowViewBinding()
{
    detach();
}

bool WindowViewBinding::canBind(QObject *model)
{
    const auto *screen = qobject_cast<ScreenViewModel *>(model);
    return !screen || !screen->m_windowView;
}

void WindowViewBinding::detach()
{
    // 旧窗口销毁后允许重新绑定；旧绑定的清理不能清除新关联。
    if (m_model && m_model->m_windowBinding == this) {
        m_model->m_windowView.clear();
        m_model->m_windowBinding = nullptr;
        m_model->m_windowIsDialog = false;
    }
    m_model.clear();
}
