#include "ShellViewModel.h"
#include <stdexcept>
#include <utility>

ShellViewModel::ShellViewModel(HomeViewModelFactory homeFactory)
    : m_homeFactory(std::move(homeFactory))
{
    if (!m_homeFactory)
        throw std::invalid_argument("Shell 要求非空的 Home 工厂");
    createHome();
}

void ShellViewModel::createHome()
{
    auto home = m_homeFactory();
    if (!home || !activateItem(std::move(home)))
        throw std::invalid_argument("Shell 工厂必须返回可接管的非空 Home");
}

void ShellViewModel::onActivate()
{
    if (!activeItem())
        createHome();
    Conductor<ScreenViewModel>::onActivate();
}
