#include <CaliburnMicroQt/ViewModelBase.h>
#include "FrameworkLog.h"

ViewModelBase::ViewModelBase(QObject *parent) : QObject(parent) {}

void ViewModelBase::reportInvalidNotify() const
{
    qCWarning(FrameworkLog::core, "ViewModelBase::setAndNotify: null signal or incompatible owner type");
}
