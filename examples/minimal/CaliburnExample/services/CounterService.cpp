#include "CounterService.h"
#include "ExampleLog.h"

void CounterService::add(int delta)
{
    // 先比较剩余额度，避免极大参数引起整数溢出。
    if (!canAdd(delta)) {
        qCDebug(exampleLog) << "拒绝增加计数" << delta << "当前值" << m_count;
        return;
    }
    m_count += delta;
    qCInfo(exampleLog) << "增加计数" << delta << "当前值" << m_count;
    emit countChanged();
}

void CounterService::reset()
{
    if (m_count == 0)
        return;
    m_count = 0;
    qCInfo(exampleLog) << "计数已重置";
    emit countChanged();
}
