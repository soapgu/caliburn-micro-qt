#include <CaliburnMicroQt/ConductorViewModelBase.h>
#include <QQmlEngine>
#include <utility>

namespace {
void activateScreen(ViewModelBase *item)
{
    if (auto *screen = qobject_cast<ScreenViewModel *>(item))
        screen->activate();
}
void deactivateScreen(ViewModelBase *item, bool close)
{
    if (auto *screen = qobject_cast<ScreenViewModel *>(item))
        screen->deactivate(close);
}
}

ConductorViewModelBase::ConductorViewModelBase(QObject *parent) : ConductorBase(parent) {}

ConductorViewModelBase::~ConductorViewModelBase()
{
    // 父树随后回收当前、留存及等待删除的对象；析构不发送关系通知。
    for (const auto &connection : std::as_const(m_destroyed))
        QObject::disconnect(connection);
}

QList<ViewModelBase *> ConductorViewModelBase::getChildren() const
{
    return m_activeItem ? QList<ViewModelBase *>{m_activeItem.data()} : QList<ViewModelBase *>{};
}

bool ConductorViewModelBase::validateItemChange(ViewModelBase *item) const
{
    if (!item)
        return true;
    if (item->parent() || item->thread() != thread()) {
        qWarning("Conductor：接管对象必须无父对象且与 Conductor 位于同一线程");
        return false;
    }
    // 防止接管自身或拥有当前 Conductor 的祖先，形成 QObject 父树环。
    for (auto *ancestor = static_cast<const QObject *>(this); ancestor; ancestor = ancestor->parent()) {
        if (ancestor == item) {
            qWarning("Conductor：不能接管自身或祖先对象");
            return false;
        }
    }
    if (auto *screen = qobject_cast<ScreenViewModel *>(item); screen && screen->isActive()) {
        qWarning("Conductor：不能接管已经激活的 Screen");
        return false;
    }
    if (hasLogicalParent(item)) {
        qWarning("Conductor：不能接管已有逻辑 Parent 的对象");
        return false;
    }
    return true;
}

void ConductorViewModelBase::changeActiveItem(std::unique_ptr<ViewModelBase> item)
{
    auto *next = item.get();
    if (next) {
        next->setParent(this);
        QQmlEngine::setObjectOwnership(next, QQmlEngine::CppOwnership);
        item.release();
        m_ownedItems.append(next);
        m_destroyed.insert(next, connect(next, &QObject::destroyed, this, [this, next] {
            memberDestroyed(next);
        }));
    }
    selectOwnedItem(next);
}

bool ConductorViewModelBase::activateItem(ViewModelBase *item)
{
    if (item && !m_ownedItems.contains(item)) {
        onActivationProcessed(item, false);
        return false;
    }
    selectOwnedItem(item);
    return true;
}

void ConductorViewModelBase::forgetItem(ViewModelBase *item)
{
    QObject::disconnect(m_destroyed.take(item));
    m_ownedItems.removeOne(item);
}

void ConductorViewModelBase::selectOwnedItem(ViewModelBase *item)
{
    auto *previous = m_activeItem.data();
    if (previous == item) {
        if (isActive()) {
            activateScreen(item);
            onActivationProcessed(item, true);
        }
        return;
    }
    m_activeItem = item;
    m_activeIdentity = item;
    if (previous)
        forgetItem(previous);
    setLogicalParent(previous, nullptr);
    setLogicalParent(item, this);
    emit activeItemChanged();
    deactivateScreen(previous, true);
    if (isActive())
        activateScreen(item);
    if (previous)
        previous->deleteLater();
    onActivationProcessed(item, true);
}

bool ConductorViewModelBase::deactivateItem(ViewModelBase *item, bool close)
{
    if (!item || !m_ownedItems.contains(item))
        return false;
    if (close) {
        closeOwnedItem(item);
        return true;
    }
    if (m_activeIdentity == item) {
        m_activeItem.clear();
        m_activeIdentity = nullptr;
        emit activeItemChanged();
    }
    deactivateScreen(item, false);
    return true;
}

void ConductorViewModelBase::closeOwnedItem(ViewModelBase *item)
{
    const bool selected = m_activeIdentity == item;
    forgetItem(item);
    if (selected) {
        m_activeItem.clear();
        m_activeIdentity = nullptr;
    }
    setLogicalParent(item, nullptr);
    if (selected)
        emit activeItemChanged();
    deactivateScreen(item, true);
    item->deleteLater();
}

void ConductorViewModelBase::memberDestroyed(ViewModelBase *identity)
{
    m_destroyed.remove(identity);
    m_ownedItems.removeOne(identity);
    if (m_activeIdentity == identity) {
        m_activeItem.clear();
        m_activeIdentity = nullptr;
        emit activeItemChanged();
    }
}

void ConductorViewModelBase::onActivate()
{
    activateScreen(m_activeItem.data());
}

void ConductorViewModelBase::onDeactivate(bool close)
{
    if (!close) {
        deactivateScreen(m_activeItem.data(), false);
        return;
    }
    const auto previous = m_ownedItems;
    const bool hadSelection = m_activeIdentity != nullptr;
    for (const auto &connection : std::as_const(m_destroyed))
        QObject::disconnect(connection);
    m_destroyed.clear();
    m_ownedItems.clear();
    m_activeItem.clear();
    m_activeIdentity = nullptr;
    for (auto *item : previous)
        setLogicalParent(item, nullptr);
    if (hadSelection)
        emit activeItemChanged();
    for (auto *item : previous) {
        deactivateScreen(item, true);
        item->deleteLater();
    }
}
