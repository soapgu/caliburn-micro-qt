#pragma once

#include <QtPlugin>

// 同步生命周期能力；调用方遵循主线程及不重入约定。
class IActivate
{
public:
    virtual ~IActivate() = default;
    virtual bool isActive() const = 0;
    virtual void activate() = 0;
};

Q_DECLARE_INTERFACE(IActivate, "Caliburn.Micro.Qt.IActivate/1.0")
