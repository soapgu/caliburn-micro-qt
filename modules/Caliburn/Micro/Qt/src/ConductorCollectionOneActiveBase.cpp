#include <CaliburnMicroQt/ConductorCollectionOneActiveBase.h>
#include <CaliburnMicroQt/ScreenExtensions.h>
#include <QQmlEngine>
#include <utility>

ConductorCollectionOneActiveBase::ConductorCollectionOneActiveBase(QObject *parent)
    : ConductorBaseWithActiveItem(parent) {}

ConductorCollectionOneActiveBase::~ConductorCollectionOneActiveBase()
{
    for (const auto &connection : std::as_const(m_destroyed))
        QObject::disconnect(connection);
}

QVariantList ConductorCollectionOneActiveBase::items() const
{
    QVariantList result;
    for (auto *item : m_items)
        result.append(QVariant::fromValue(item));
    return result;
}

bool ConductorCollectionOneActiveBase::validateItemChange(ViewModelBase *item) const
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

void ConductorCollectionOneActiveBase::addMember(ViewModelBase *item)
{
    m_items.append(item);
    m_destroyed.insert(item, connect(item, &QObject::destroyed, this, [this, item] {
        memberDestroyed(item);
    }));
    setLogicalParent(item, this);
    emit itemsChanged();
}

void ConductorCollectionOneActiveBase::removeMember(ViewModelBase *item)
{
    if (!m_items.removeOne(item)) return;
    QObject::disconnect(m_destroyed.take(item));
    setLogicalParent(item, nullptr);
    emit itemsChanged();
}

void ConductorCollectionOneActiveBase::adoptItem(std::unique_ptr<ViewModelBase> item, bool select)
{
    auto *next = item.get();
    next->setParent(this);
    QQmlEngine::setObjectOwnership(next, QQmlEngine::CppOwnership);
    item.release();
    if (select) activateItem(next);
    else addMember(next);
}

void ConductorCollectionOneActiveBase::activateItem(ViewModelBase *item)
{
    // Qt 所有权边界：新对象由 unique_ptr 接管；原先接管的对象可再次加入集合。
    if (item && item->parent() != this) {
        onActivationProcessed(item, false);
        return;
    }
    if (item && item == activeItem()) {
        if (isActive()) {
            ScreenExtensions::tryActivate(item);
            onActivationProcessed(item, true);
        }
        return;
    }
    changeActiveItem(item, false);
}

void ConductorCollectionOneActiveBase::deactivateItem(ViewModelBase *item, bool close)
{
    if (!item || item->parent() != this) return;
    if (!close) { ScreenExtensions::tryDeactivate(item, false); return; }
    QPointer<ViewModelBase> target = item;
    checkClose({item}, [this, target](bool allowed) {
        if (!allowed || !target) return;
        if (!prepareCloseItem(target.data())) return;
        if (target) closeMember(target.data());
    });
}

ViewModelBase *ConductorCollectionOneActiveBase::determineNextItemToActivate(
        const QList<ViewModelBase *> &items, int lastIndex)
{
    const int previous = lastIndex - 1;
    if (previous == -1 && items.size() > 1) return items.at(1);
    if (previous > -1 && previous < items.size() - 1) return items.at(previous);
    return nullptr;
}

ViewModelBase *ConductorCollectionOneActiveBase::ensureItem(ViewModelBase *item)
{
    if (!item)
        item = determineNextItemToActivate(m_items, activeItem() ? m_items.indexOf(activeItem()) : 0);
    else if (!m_items.contains(item))
        addMember(item);
    return ConductorBase::ensureItem(item);
}

bool ConductorCollectionOneActiveBase::closeMember(ViewModelBase *item)
{
    if (!item) return false;
    if (item == activeItem())
        changeActiveItem(determineNextItemToActivate(m_items, m_items.indexOf(item)), true);
    else
        ScreenExtensions::tryDeactivate(item, true);
    // CM 在关闭与选择通知完成后才移除旧项并清空它的逻辑 Parent。
    removeMember(item);
    return true;
}

void ConductorCollectionOneActiveBase::memberDestroyed(ViewModelBase *identity)
{
    m_destroyed.remove(identity);
    m_items.removeOne(identity);
    emit itemsChanged();
    // 当前项销毁由公共基类清空并通知，不自动导航。
}

void ConductorCollectionOneActiveBase::canClose(CloseCallback callback)
{
    checkCloseResult(m_items, [this, callback = std::move(callback)](bool allowed, const auto &closable) {
        if (!allowed && !closable.isEmpty()) {
            auto remaining = closable;
            if (remaining.contains(activeItem())) {
                auto candidates = m_items;
                auto *next = activeItem();
                do {
                    auto *previous = next;
                    next = determineNextItemToActivate(candidates, candidates.indexOf(previous));
                    candidates.removeOne(previous);
                } while (remaining.contains(next));
                auto *previous = activeItem();
                changeActiveItem(next, true);
                removeMember(previous);
                remaining.removeOne(previous);
            }
            for (auto *item : remaining)
                ScreenExtensions::tryDeactivate(item, true);
            for (auto *item : remaining)
                removeMember(item);
        }
        callback(allowed);
    });
}

void ConductorCollectionOneActiveBase::onActivate()
{
    ScreenExtensions::tryActivate(activeItem());
}

void ConductorCollectionOneActiveBase::onDeactivate(bool close)
{
    if (!close) {
        ScreenExtensions::tryDeactivate(activeItem(), false);
        return;
    }
    for (auto *item : std::as_const(m_items))
        ScreenExtensions::tryDeactivate(item, true);
    // 对齐 CM：Clear 集合不清空 ActiveItem，也不执行逐项 Remove 的 Parent 清理。
    for (const auto &connection : std::as_const(m_destroyed))
        QObject::disconnect(connection);
    m_destroyed.clear();
    m_items.clear();
    emit itemsChanged();
}
