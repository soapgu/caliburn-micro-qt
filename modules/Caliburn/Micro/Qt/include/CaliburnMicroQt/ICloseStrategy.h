#pragma once

#include <CaliburnMicroQt/IGuardClose.h>
#include <QList>

class ViewModelBase;

class ICloseStrategy
{
public:
    virtual ~ICloseStrategy() = default;
    virtual void execute(const QList<ViewModelBase *> &items, CloseCallback callback) = 0;
};
