#pragma once

#include <CaliburnMicroQt/ScreenViewModel.h>
#include <CaliburnMicroQt/IConductor.h>

class ConductorBase : public ScreenViewModel, public IConductor
{
    Q_OBJECT
    Q_INTERFACES(IParent IConductor)
    QML_ELEMENT
    QML_UNCREATABLE("抽象 Conductor 不能直接创建")

public:
    using ScreenViewModel::ScreenViewModel;

signals:
    void activationProcessed(ViewModelBase *item, bool success);

protected:
    static bool hasLogicalParent(ViewModelBase *item);
    static void setLogicalParent(ViewModelBase *item, QObject *parent);
    void onActivationProcessed(ViewModelBase *item, bool success);
};
