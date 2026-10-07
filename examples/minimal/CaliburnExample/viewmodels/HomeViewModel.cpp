#include "HomeViewModel.h"
#include <stdexcept>
#include <utility>

HomeViewModel::HomeViewModel(std::shared_ptr<CounterService> counterService)
    : m_counterService(std::move(counterService))
{
    if (!m_counterService)
        throw std::invalid_argument("Home 要求非空的计数服务");
    m_lastCanIncrement = canIncrement();
    m_lastCanAddTwo = canAddTwo();
    m_lastCanReset = canReset();
    connect(m_counterService.get(), &CounterService::countChanged,
            this, &HomeViewModel::notifyCountChanged);
}

QString HomeViewModel::message() const
{
    return QStringLiteral("已点击 %1 次").arg(count());
}

void HomeViewModel::increment()
{
    add(1);
}

void HomeViewModel::add(int delta)
{
    if (m_counterService->canAdd(delta))
        m_counterService->add(delta);
}

void HomeViewModel::reset()
{
    if (canReset())
        m_counterService->reset();
}

void HomeViewModel::notifyCountChanged()
{
    const bool incrementChanged = m_lastCanIncrement != canIncrement();
    const bool addTwoChanged = m_lastCanAddTwo != canAddTwo();
    const bool resetChanged = m_lastCanReset != canReset();
    m_lastCanIncrement = canIncrement();
    m_lastCanAddTwo = canAddTwo();
    m_lastCanReset = canReset();
    emit countChanged();
    if (incrementChanged)
        emit canIncrementChanged();
    if (addTwoChanged)
        emit canAddTwoChanged();
    if (resetChanged)
        emit canResetChanged();
}
