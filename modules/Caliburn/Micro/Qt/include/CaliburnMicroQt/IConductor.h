#pragma once

#include <CaliburnMicroQt/IParent.h>

class IConductor : public IParent
{
public:
    ~IConductor() override = default;
    virtual bool activateItem(ViewModelBase *item) = 0;
    virtual bool deactivateItem(ViewModelBase *item, bool close) = 0;
};

Q_DECLARE_INTERFACE(IConductor, "Caliburn.Micro.Qt.IConductor/1.0")
