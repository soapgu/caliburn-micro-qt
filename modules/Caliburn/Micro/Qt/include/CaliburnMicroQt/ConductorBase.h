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
    QML_UNCREATABLE("Abstract conductor cannot be created directly")

public:
    explicit ConductorBase(QObject *parent = nullptr);
    std::shared_ptr<ICloseStrategy> closeStrategy() const;
    void setCloseStrategy(std::shared_ptr<ICloseStrategy> strategy);
    QList<QObject *> getChildren() const override = 0;
    void activateItem(QObject *item) override = 0;
    void deactivateItem(QObject *item, bool close) override = 0;

signals:
    void activationProcessed(QObject *item, bool success);

protected:
    void checkClose(const QList<QObject *> &items, CloseCallback continuation);
    void checkCloseResult(const QList<QObject *> &items, CloseStrategyCallback continuation);
    virtual QObject *ensureItem(QObject *item);
    virtual bool acceptsItem(QObject *item) const { Q_UNUSED(item); return true; }
    static bool hasLogicalParent(QObject *item);
    static void setLogicalParent(QObject *item, QObject *parent);
    virtual void onActivationProcessed(QObject *item, bool success);
private:
    mutable std::shared_ptr<ICloseStrategy> m_closeStrategy;
};
