#include "ShellViewModel.h"
#include <stdexcept>
#include <utility>

ShellViewModel::ShellViewModel(HomeViewModelFactory homeFactory, DetailViewModelFactory detailFactory)
    : m_homeFactory(std::move(homeFactory)), m_detailFactory(std::move(detailFactory))
{
    if (!m_homeFactory || !m_detailFactory)
        throw std::invalid_argument("Shell requires non-empty Home and Detail factories");
    auto *initial = ensureHome();
    activateItem(initial);
    if (activeItem() != initial)
        throw std::invalid_argument("Shell could not select the initial Home");
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
        throw std::invalid_argument("Shell factory must return a non-null adoptable Home");
    return raw;
}

void ShellViewModel::showDetail()
{
    if (!canShowDetail()) return;
    if (auto *existing = detail()) { activateItem(existing); return; }
    auto next = m_detailFactory();
    auto *raw = next.get();
    if (!next) throw std::invalid_argument("Shell factory must return a non-null adoptable Detail");
    activateItem(std::move(next));
    if (activeItem() != raw)
        throw std::invalid_argument("Shell factory must return a non-null adoptable Detail");
}

bool ShellViewModel::prepareCloseItem(QObject *item)
{
    if (item && item == detail() && item == activeItem()) ensureHome();
    return true;
}

void ShellViewModel::onActivate()
{
    auto *availableHome = ensureHome();
    if (!activeItem() || !items().contains(activeItem())) {
        activateItem(availableHome);
        if (activeItem() != availableHome)
            throw std::invalid_argument("Shell could not select Home");
        // Shell 已活动，集合选择入口已经完成子项激活。
        return;
    }
    Conductor<ScreenViewModel>::Collection::OneActive::onActivate();
}
