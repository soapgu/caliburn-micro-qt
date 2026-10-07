#include "CounterService.h"

void CounterService::add(int delta)
{
    // 先比较剩余额度，避免极大参数引起整数溢出。
    if (!canAdd(delta))
        return;
    m_count += delta;
    emit countChanged();
}

void CounterService::reset()
{
    if (m_count == 0)
        return;
    m_count = 0;
    emit countChanged();
}
