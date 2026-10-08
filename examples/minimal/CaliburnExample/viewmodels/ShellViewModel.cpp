#include "ShellViewModel.h"
#include <stdexcept>
#include <utility>

ShellViewModel::ShellViewModel(HomeViewModelFactory homeFactory, DetailViewModelFactory detailFactory)
    : m_homeFactory(std::move(homeFactory)), m_detailFactory(std::move(detailFactory))
{
    if (!m_homeFactory || !m_detailFactory)
        throw std::invalid_argument("Shell 要求非空的 Home 与 Detail 工厂");
    auto *initial = ensureHome();
    if (!activateItem(initial))
        throw std::invalid_argument("Shell 无法选择初始 Home");
    connect(this, &ShellViewModel::itemsChanged, this, &ShellViewModel::navigationChanged);
    connect(this, &ShellViewModel::activeItemChanged, this, &ShellViewModel::navigationChanged);
    connect(this, &ShellViewModel::isActiveChanged, this, &ShellViewModel::navigationChanged);
}

HomeViewModel *ShellViewModel::home() const
{
    for (auto *item : items())
        if (auto *home = qobject_cast<HomeViewModel *>(item))
            return home;
    return nullptr;
}

DetailViewModel *ShellViewModel::detail() const
{
    for (auto *item : items())
        if (auto *detail = qobject_cast<DetailViewModel *>(item))
            return detail;
    return nullptr;
}

HomeViewModel *ShellViewModel::ensureHome()
{
    if (auto *existing = home())
        return existing;
    auto home = m_homeFactory();
    auto *raw = home.get();
    if (!home || !addItem(std::move(home)))
        throw std::invalid_argument("Shell 工厂必须返回可接管的非空 Home");
    return raw;
}

bool ShellViewModel::showDetail()
{
    if (!canShowDetail())
        return false;
    if (auto *existing = detail())
        return activateItem(existing);
    auto next = m_detailFactory();
    if (!next || !activateItem(std::move(next)))
        throw std::invalid_argument("Shell 工厂必须返回可接管的非空 Detail");
    return true;
}

bool ShellViewModel::deactivateItem(ViewModelBase *item, bool close)
{
    // 先补齐返回目标；工厂失败时尚未移除或关闭 Detail。
    if (close && item && item == detail() && item == activeItem())
        ensureHome();
    return ConductorCollectionOneActiveViewModelBase::deactivateItem(item, close);
}

void ShellViewModel::onActivate()
{
    auto *availableHome = ensureHome();
    if (!activeItem()) {
        if (!activateItem(availableHome))
            throw std::invalid_argument("Shell 无法选择 Home");
    }
    Conductor<ScreenViewModel>::Collection::OneActive::onActivate();
}
