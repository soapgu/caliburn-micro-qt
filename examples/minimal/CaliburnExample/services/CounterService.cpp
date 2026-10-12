#include "CounterService.h"
#include "ExampleLog.h"

void CounterService::add(int delta)
{
    // 先比较剩余额度，避免极大参数引起整数溢出。
    if (!canAdd(delta)) {
        qCDebug(exampleLog) << "Counter increment rejected; delta:" << delta << "current value:" << m_count;
        return;
    }
    m_count += delta;
    qCInfo(exampleLog) << "Counter incremented; delta:" << delta << "current value:" << m_count;
    emit countChanged();
}

void CounterService::reset()
{
    if (m_count == 0)
        return;
    m_count = 0;
    qCInfo(exampleLog) << "Counter reset";
    emit countChanged();
}
