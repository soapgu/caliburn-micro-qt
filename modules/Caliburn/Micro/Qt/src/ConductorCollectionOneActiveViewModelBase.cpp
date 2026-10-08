#include <CaliburnMicroQt/ConductorCollectionOneActiveViewModelBase.h>
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
    if (auto *screen = qobject_cast<ScreenViewModel *>(item); screen && screen->isActive()) {
        qWarning("Collection.OneActive：不能接管已经激活的 Screen");
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
        deactivateScreen(previous, false);
        if (isActive())
            activateScreen(next);
        onActivationProcessed(next, true);
    }
}

bool ConductorCollectionOneActiveViewModelBase::activateItem(ViewModelBase *item)
{
    if (item && !m_items.contains(item)) {
        onActivationProcessed(item, false);
        return false;
    }
    auto *previous = m_activeItem.data();
    if (previous == item) {
        if (isActive()) {
            activateScreen(item);
            onActivationProcessed(item, true);
        }
        return true;
    }
    setSelection(item);
    emit activeItemChanged();
    deactivateScreen(previous, false);
    if (isActive())
        activateScreen(item);
    onActivationProcessed(item, true);
    return true;
}

bool ConductorCollectionOneActiveViewModelBase::deactivateItem(ViewModelBase *item, bool close)
{
    if (!item || !m_items.contains(item))
        return false;
    if (close)
        return closeMember(item);
    deactivateScreen(item, false);
    return true;
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
    deactivateScreen(item, true);
    if (selected && isActive())
        activateScreen(next);
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
    activateScreen(m_activeItem.data());
}

void ConductorCollectionOneActiveViewModelBase::onDeactivate(bool close)
{
    if (!close) {
        deactivateScreen(m_activeItem.data(), false);
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
        deactivateScreen(item, true);
        item->deleteLater();
    }
}
