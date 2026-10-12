#include "FrameworkLog.h"
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
    qCDebug(FrameworkLog::conductor).noquote() << "Checking close permission:" << FrameworkLog::identity(this) << "items:" << items.size();
    const auto identity = FrameworkLog::identity(this);
    auto strategy = closeStrategy();
    QPointer<ConductorBase> owner = this;
    strategy->executeWithResult(items, [owner, identity, continuation = std::move(continuation)](bool allowed, const auto &closable) mutable {
        auto complete = std::move(continuation);
        if (!owner) {
            qCDebug(FrameworkLog::conductor).noquote() << "Close result discarded; conductor destroyed:" << identity;
            return;
        }
        qCInfo(FrameworkLog::conductor).noquote() << "Close permission resolved:" << identity << "allowed:" << allowed << "closable items:" << closable.size();
        complete(allowed, closable);
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
    qCDebug(FrameworkLog::conductor).noquote() << "Activation processed:" << FrameworkLog::identity(this)
                                            << "item:" << FrameworkLog::identity(item) << "success:" << success;
    if (item)
        emit activationProcessed(item, success);
}
