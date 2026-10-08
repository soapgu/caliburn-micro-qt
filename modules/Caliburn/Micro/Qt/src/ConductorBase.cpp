#include <CaliburnMicroQt/ConductorBase.h>

bool ConductorBase::hasLogicalParent(ViewModelBase *item)
{
    auto *child = qobject_cast<IChild *>(item);
    return child && child->parentViewModel();
}

void ConductorBase::setLogicalParent(ViewModelBase *item, QObject *parent)
{
    if (auto *child = qobject_cast<IChild *>(item))
        child->setParentViewModel(parent);
}

void ConductorBase::onActivationProcessed(ViewModelBase *item, bool success)
{
    if (item)
        emit activationProcessed(item, success);
}
