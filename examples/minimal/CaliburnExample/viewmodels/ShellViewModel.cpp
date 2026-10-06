#include "ShellViewModel.h"

ShellViewModel::ShellViewModel(QObject *parent) : ViewModelBase(parent) {}

QString ShellViewModel::message() const
{
    return QStringLiteral("已点击 %1 次").arg(m_count);
}

void ShellViewModel::increment()
{
    add(1);
}

void ShellViewModel::add(int delta)
{
    // 先比较剩余额度，再相加，避免极大参数导致有符号整数溢出。
    if (canAdd(delta))
        updateCount(m_count + delta);
}

void ShellViewModel::reset()
{
    if (canReset())
        updateCount(0);
}

void ShellViewModel::updateCount(int value)
{
    const bool oldCanIncrement = canIncrement();
    const bool oldCanAddTwo = canAddTwo();
    const bool oldCanReset = canReset();
    if (!setAndNotify(m_count, value, &ShellViewModel::countChanged))
        return;
    if (oldCanIncrement != canIncrement())
        emit canIncrementChanged();
    if (oldCanAddTwo != canAddTwo())
        emit canAddTwoChanged();
    if (oldCanReset != canReset())
        emit canResetChanged();
}
