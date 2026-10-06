#include "HomeViewModel.h"

HomeViewModel::HomeViewModel() = default;

QString HomeViewModel::message() const
{
    return QStringLiteral("已点击 %1 次").arg(m_count);
}

void HomeViewModel::increment()
{
    add(1);
}

void HomeViewModel::add(int delta)
{
    // 先比较剩余额度，再相加，避免极大参数导致有符号整数溢出。
    if (canAdd(delta))
        updateCount(m_count + delta);
}

void HomeViewModel::reset()
{
    if (canReset())
        updateCount(0);
}

void HomeViewModel::updateCount(int value)
{
    const bool oldCanIncrement = canIncrement();
    const bool oldCanAddTwo = canAddTwo();
    const bool oldCanReset = canReset();
    if (!setAndNotify(m_count, value, &HomeViewModel::countChanged))
        return;
    if (oldCanIncrement != canIncrement())
        emit canIncrementChanged();
    if (oldCanAddTwo != canAddTwo())
        emit canAddTwoChanged();
    if (oldCanReset != canReset())
        emit canResetChanged();
}
