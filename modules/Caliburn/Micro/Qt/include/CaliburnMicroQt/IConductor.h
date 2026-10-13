#pragma once

#include <CaliburnMicroQt/IParent.h>

class IConductor : public IParent
{
public:
    ~IConductor() override = default;
    virtual void activateItem(QObject *item) = 0;
    virtual void deactivateItem(QObject *item, bool close) = 0;
};

Q_DECLARE_INTERFACE(IConductor, "Caliburn.Micro.Qt.IConductor/3.0")
