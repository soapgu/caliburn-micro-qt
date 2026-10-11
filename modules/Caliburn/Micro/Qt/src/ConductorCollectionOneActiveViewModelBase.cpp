#include <CaliburnMicroQt/ConductorCollectionOneActiveViewModelBase.h>
#include <CaliburnMicroQt/ScreenExtensions.h>
#include <QQmlEngine>
#include <utility>

ConductorCollectionOneActiveViewModelBase::ConductorCollectionOneActiveViewModelBase(QObject *parent)
    : ConductorBase(parent) {}

ConductorCollectionOneActiveViewModelBase::~ConductorCollectionOneActiveViewModelBase()
{
    for (const auto &connection : std::as_const(m_destroyed))
        QObject::disconnect(connection);
}

QVariantList ConductorCollectionOneActiveViewModelBase::items() const
{
    QVariantList result;
    for (auto *item : m_items)
        result.append(QVariant::fromValue(item));
    return result;
}

bool ConductorCollectionOneActiveViewModelBase::validateItemChange(ViewModelBase *item) const
{
    if (item->parent() || item->thread() != thread()) {
        qWarning("Collection.OneActive：接管对象必须无父对象且位于同一线程");
        return false;
    }
    for (auto *ancestor = static_cast<const QObject *>(this); ancestor; ancestor = ancestor->parent()) {
        if (ancestor == item) {
            qWarning("Collection.OneActive：不能接管自身或祖先对象");
            return false;
        }
    }
    if (auto *activator = qobject_cast<IActivate *>(item); activator && activator->isActive()) {
        qWarning("Collection.OneActive：不能接管已经激活的对象");
        return false;
    }
    if (hasLogicalParent(item)) {
        qWarning("Collection.OneActive：不能接管已有逻辑 Parent 的对象");
        return false;
    }
    return true;
}

void ConductorCollectionOneActiveViewModelBase::setSelection(ViewModelBase *item)
{
    m_activeItem = item;
    m_activeIdentity = item;
}

void ConductorCollectionOneActiveViewModelBase::adoptItem(std::unique_ptr<ViewModelBase> item, bool select)
{
    auto *next = item.get();
    next->setParent(this);
    QQmlEngine::setObjectOwnership(next, QQmlEngine::CppOwnership);
    item.release();
    m_items.append(next);
    m_destroyed.insert(next, connect(next, &QObject::destroyed, this, [this, next] {
        memberDestroyed(next);
    }));
    auto *previous = m_activeItem.data();
    if (select)
        setSelection(next);
    setLogicalParent(next, this);
    emit itemsChanged();
    if (select) {
        emit activeItemChanged();
        ScreenExtensions::tryDeactivate(previous, false);
        if (isActive())
            ScreenExtensions::tryActivate(next);
        onActivationProcessed(next, true);
    }
}

void ConductorCollectionOneActiveViewModelBase::activateItem(ViewModelBase *item)
{
    if (item && !m_items.contains(item)) {
        onActivationProcessed(item, false);
        return;
    }
    auto *previous = m_activeItem.data();
    if (previous == item) {
        if (isActive()) {
            ScreenExtensions::tryActivate(item);
            onActivationProcessed(item, true);
        }
        return;
    }
    setSelection(item);
    emit activeItemChanged();
    ScreenExtensions::tryDeactivate(previous, false);
    if (isActive())
        ScreenExtensions::tryActivate(item);
    onActivationProcessed(item, true);
    return;
}

void ConductorCollectionOneActiveViewModelBase::deactivateItem(ViewModelBase *item, bool close)
{
    if (!item || !m_items.contains(item)) return;
    if (!close) { ScreenExtensions::tryDeactivate(item, false); return; }
    QPointer<ViewModelBase> target = item;
    checkClose({item}, [this, target](bool allowed) {
        if (!allowed || !target || !m_items.contains(target.data())) return;
        if (!prepareCloseItem(target.data())) return;
        // 准备钩子可能补入成员，重新定位目标并沿用相邻项规则。
        if (target && m_items.contains(target.data())) closeMember(target.data());
    });
}

bool ConductorCollectionOneActiveViewModelBase::closeMember(ViewModelBase *item)
{
    const auto index = m_items.indexOf(item);
    if (index < 0)
        return false;
    const bool selected = m_activeIdentity == item;
    ViewModelBase *next = nullptr;
    if (selected) {
        if (index > 0)
            next = m_items.at(index - 1);
        else if (m_items.size() > 1)
            next = m_items.at(1);
    }
    QObject::disconnect(m_destroyed.take(item));
    m_items.removeAt(index);
    if (selected)
        setSelection(next);
    setLogicalParent(item, nullptr);
    emit itemsChanged();
    if (selected)
        emit activeItemChanged();
    ScreenExtensions::tryDeactivate(item, true);
    if (selected && isActive())
        ScreenExtensions::tryActivate(next);
    item->deleteLater();
    if (selected)
        onActivationProcessed(next, true);
    return true;
}

void ConductorCollectionOneActiveViewModelBase::memberDestroyed(ViewModelBase *identity)
{
    const bool selected = m_activeIdentity == identity;
    m_destroyed.remove(identity);
    m_items.removeOne(identity);
    if (selected)
        setSelection(nullptr);
    emit itemsChanged();
    if (selected)
        emit activeItemChanged();
}

void ConductorCollectionOneActiveViewModelBase::onActivate()
{
    ScreenExtensions::tryActivate(m_activeItem.data());
}

void ConductorCollectionOneActiveViewModelBase::onDeactivate(bool close)
{
    if (!close) {
        ScreenExtensions::tryDeactivate(m_activeItem.data(), false);
        return;
    }
    const auto previous = m_items;
    const bool hadSelection = m_activeIdentity != nullptr;
    for (const auto &connection : std::as_const(m_destroyed))
        QObject::disconnect(connection);
    m_destroyed.clear();
    m_items.clear();
    setSelection(nullptr);
    for (auto *item : previous)
        setLogicalParent(item, nullptr);
    if (!previous.isEmpty())
        emit itemsChanged();
    if (hadSelection)
        emit activeItemChanged();
    for (auto *item : previous) {
        ScreenExtensions::tryDeactivate(item, true);
        item->deleteLater();
    }
}
