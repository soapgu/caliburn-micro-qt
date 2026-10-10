#pragma once

#include <QtPlugin>
#include <functional>

using CloseCallback = std::function<void(bool)>;

class IGuardClose
{
public:
    virtual ~IGuardClose() = default;
    // 主线程调用；实现必须立即或延后交付一次许可。
    virtual void canClose(CloseCallback callback) = 0;
};

Q_DECLARE_INTERFACE(IGuardClose, "Caliburn.Micro.Qt.IGuardClose/1.0")
