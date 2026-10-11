#include <CaliburnMicroQt/ConductorBase.h>
#include <CaliburnMicroQt/DefaultCloseStrategy.h>
#include <utility>

ConductorBase::ConductorBase(QObject *parent) : ScreenViewModel(parent) {}

std::shared_ptr<ICloseStrategy> ConductorBase::closeStrategy() const
{
    if (!m_closeStrategy)
        m_closeStrategy = std::make_shared<DefaultCloseStrategy>();
    return m_closeStrategy;
}

void ConductorBase::setCloseStrategy(std::shared_ptr<ICloseStrategy> strategy)
{
    m_closeStrategy = std::move(strategy);
}

void ConductorBase::checkClose(const QList<ViewModelBase *> &items, CloseCallback continuation)
{
    checkCloseResult(items, [continuation = std::move(continuation)](bool allowed, const auto &) {
        continuation(allowed);
    });
}

void ConductorBase::checkCloseResult(const QList<ViewModelBase *> &items, CloseStrategyCallback continuation)
{
    auto strategy = closeStrategy();
    QPointer<ConductorBase> owner = this;
    strategy->executeWithResult(items, [owner, continuation = std::move(continuation)](bool allowed, const auto &closable) mutable {
        auto complete = std::move(continuation);
        if (owner) complete(allowed, closable);
    });
}

ViewModelBase *ConductorBase::ensureItem(ViewModelBase *item)
{
    if (auto *child = qobject_cast<IChild *>(item); child && child->parentViewModel() != this)
        setLogicalParent(item, this);
    return item;
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
