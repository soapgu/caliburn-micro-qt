#include "ShellViewModel.h"

ShellViewModel::ShellViewModel(QObject *parent) : ViewModelBase(parent) {}

QString ShellViewModel::message() const
{
    return QStringLiteral("已点击 %1 次").arg(m_count);
}

void ShellViewModel::increment()
{
    if (canIncrement())
        updateCount(m_count + 1);
}

void ShellViewModel::reset()
{
    if (canReset())
        updateCount(0);
}

void ShellViewModel::updateCount(int value)
{
    const bool oldCanIncrement = canIncrement();
    const bool oldCanReset = canReset();
    if (!setAndNotify(m_count, value, &ShellViewModel::countChanged))
        return;
    if (oldCanIncrement != canIncrement())
        emit canIncrementChanged();
    if (oldCanReset != canReset())
        emit canResetChanged();
}
