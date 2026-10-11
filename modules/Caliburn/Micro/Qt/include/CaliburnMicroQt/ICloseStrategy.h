#pragma once

#include <CaliburnMicroQt/IGuardClose.h>
#include <QList>
#include <utility>

class ViewModelBase;
using CloseStrategyCallback = std::function<void(bool, const QList<ViewModelBase *> &)>;

class ICloseStrategy
{
public:
    virtual ~ICloseStrategy() = default;
    virtual void execute(const QList<ViewModelBase *> &items, CloseCallback callback) = 0;
    // 兼容既有 bool 策略；需要部分关闭的策略覆盖此入口。
    virtual void executeWithResult(const QList<ViewModelBase *> &items, CloseStrategyCallback callback)
    {
        execute(items, [callback = std::move(callback)](bool allowed) { callback(allowed, {}); });
    }
};
