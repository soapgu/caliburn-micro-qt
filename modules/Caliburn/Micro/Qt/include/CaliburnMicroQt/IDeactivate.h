#pragma once

#include <QtPlugin>

// close 表示关闭生命周期；对象回收仍由所有者负责。
class IDeactivate
{
public:
    virtual ~IDeactivate() = default;
    virtual void deactivate(bool close) = 0;
};

Q_DECLARE_INTERFACE(IDeactivate, "Caliburn.Micro.Qt.IDeactivate/1.0")
