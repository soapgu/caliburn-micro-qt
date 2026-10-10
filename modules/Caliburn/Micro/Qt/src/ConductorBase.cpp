#include <CaliburnMicroQt/ConductorBase.h>
#include <CaliburnMicroQt/DefaultCloseStrategy.h>
#include <utility>

ConductorBase::ConductorBase(QObject *parent) : ScreenViewModel(parent),
    m_closeStrategy(std::make_shared<DefaultCloseStrategy>()) {}

void ConductorBase::setCloseStrategy(std::shared_ptr<ICloseStrategy> strategy)
{
    m_closeStrategy = strategy ? std::move(strategy) : std::make_shared<DefaultCloseStrategy>();
}

void ConductorBase::checkClose(const QList<ViewModelBase *> &items, CloseCallback continuation)
{
    // 策略在 execute 调用期间保活；异步状态由策略实现自行持有。
    auto strategy = m_closeStrategy;
    QPointer<ConductorBase> owner = this;
    strategy->execute(items, [owner, continuation = std::move(continuation)](bool allowed) mutable {
        auto complete = std::move(continuation);
        if (owner) complete(allowed);
    });
}

void ConductorBase::canClose(CloseCallback callback)
{
    checkClose(getChildren(), std::move(callback));
}

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
