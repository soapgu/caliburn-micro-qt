#pragma once

#include <CaliburnMicroQt/ScreenViewModel.h>
#include <CaliburnMicroQt/IConductor.h>
#include <CaliburnMicroQt/ICloseStrategy.h>
#include <memory>

class ConductorBase : public ScreenViewModel, public IConductor
{
    Q_OBJECT
    Q_INTERFACES(IParent IConductor)
    QML_ELEMENT
    QML_UNCREATABLE("抽象 Conductor 不能直接创建")

public:
    explicit ConductorBase(QObject *parent = nullptr);
    std::shared_ptr<ICloseStrategy> closeStrategy() const { return m_closeStrategy; }
    void setCloseStrategy(std::shared_ptr<ICloseStrategy> strategy);
    void canClose(CloseCallback callback) override;

signals:
    void activationProcessed(ViewModelBase *item, bool success);

protected:
    void checkClose(const QList<ViewModelBase *> &items, CloseCallback continuation);
    static bool hasLogicalParent(ViewModelBase *item);
    static void setLogicalParent(ViewModelBase *item, QObject *parent);
    void onActivationProcessed(ViewModelBase *item, bool success);
private:
    std::shared_ptr<ICloseStrategy> m_closeStrategy;
};
