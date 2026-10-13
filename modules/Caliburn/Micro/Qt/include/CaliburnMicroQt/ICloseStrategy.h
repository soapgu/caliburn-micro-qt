#pragma once

#include <CaliburnMicroQt/IGuardClose.h>
#include <QList>
#include <utility>

class QObject;
using CloseStrategyCallback = std::function<void(bool, const QList<QObject *> &)>;

class ICloseStrategy
{
public:
    virtual ~ICloseStrategy() = default;
    virtual void execute(const QList<QObject *> &items, CloseCallback callback) = 0;
    // 兼容既有 bool 策略；需要部分关闭的策略覆盖此入口。
    virtual void executeWithResult(const QList<QObject *> &items, CloseStrategyCallback callback)
    {
        execute(items, [callback = std::move(callback)](bool allowed) { callback(allowed, {}); });
    }
};
