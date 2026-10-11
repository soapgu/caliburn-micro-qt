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
    std::shared_ptr<ICloseStrategy> closeStrategy() const;
    void setCloseStrategy(std::shared_ptr<ICloseStrategy> strategy);
    QList<ViewModelBase *> getChildren() const override = 0;
    void activateItem(ViewModelBase *item) override = 0;
    void deactivateItem(ViewModelBase *item, bool close) override = 0;

signals:
    void activationProcessed(ViewModelBase *item, bool success);

protected:
    void checkClose(const QList<ViewModelBase *> &items, CloseCallback continuation);
    void checkCloseResult(const QList<ViewModelBase *> &items, CloseStrategyCallback continuation);
    virtual ViewModelBase *ensureItem(ViewModelBase *item);
    static bool hasLogicalParent(ViewModelBase *item);
    static void setLogicalParent(ViewModelBase *item, QObject *parent);
    virtual void onActivationProcessed(ViewModelBase *item, bool success);
private:
    mutable std::shared_ptr<ICloseStrategy> m_closeStrategy;
};
