#pragma once

#include <CaliburnMicroQt/ICloseStrategy.h>

// 按快照顺序检查全部成员，最终汇总；不支持部分关闭。
class DefaultCloseStrategy : public ICloseStrategy
{
public:
    void execute(const QList<ViewModelBase *> &items, CloseCallback callback) override;
};
