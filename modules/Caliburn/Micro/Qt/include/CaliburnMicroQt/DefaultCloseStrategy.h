#pragma once

#include <CaliburnMicroQt/ICloseStrategy.h>

// 默认整体许可；可选择在父级不能关闭时返回允许关闭的成员。
class DefaultCloseStrategy : public ICloseStrategy
{
public:
    explicit DefaultCloseStrategy(bool closeConductedItemsWhenConductorCannotClose = false)
        : m_partialClose(closeConductedItemsWhenConductorCannotClose) {}
    void execute(const QList<ViewModelBase *> &items, CloseCallback callback) override;
    void executeWithResult(const QList<ViewModelBase *> &items, CloseStrategyCallback callback) override;
private:
    bool m_partialClose;
};
