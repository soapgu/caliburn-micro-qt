#include <CaliburnMicroQt/ConductorSingleItem.h>
#include <CaliburnMicroQt/ScreenExtensions.h>
#include <QQmlEngine>
#include <utility>

ConductorSingleItem::ConductorSingleItem(QObject *parent) : ConductorBaseWithActiveItem(parent) {}

QList<ViewModelBase *> ConductorSingleItem::getChildren() const
{
    return QList<ViewModelBase *>{activeItem()};
}

bool ConductorSingleItem::validateItemChange(ViewModelBase *item) const
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
    if (auto *activator = qobject_cast<IActivate *>(item); activator && activator->isActive()) {
        qWarning("Conductor：不能接管已经激活的对象");
        return false;
    }
    if (hasLogicalParent(item)) {
        qWarning("Conductor：不能接管已有逻辑 Parent 的对象");
        return false;
    }
    return true;
}

void ConductorSingleItem::adoptAndActivate(std::unique_ptr<ViewModelBase> item)
{
    if (!item) { activateItem(nullptr); return; }
    QQmlEngine::setObjectOwnership(item.get(), QQmlEngine::CppOwnership);
    // std::function 要求可复制捕获；共享的是所有权容器，接管时仍转出 unique_ptr。
    auto candidate = std::make_shared<std::unique_ptr<ViewModelBase>>(std::move(item));
    checkClose(getChildren(), [this, candidate](bool allowed) {
        auto owned = std::move(*candidate);
        auto *next = owned.get();
        if (!allowed) { onActivationProcessed(next, false); return; }
        next->setParent(this);
        owned.release();
        changeActiveItem(next, true);
    });
}

void ConductorSingleItem::activateItem(ViewModelBase *item)
{
    // 旧对象可由业务再次选择；从未接管的新对象必须走 unique_ptr 入口。
    if (item && (item->parent() != this
        || (qobject_cast<IChild *>(item) && qobject_cast<IChild *>(item)->parentViewModel() != this))) {
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
    QPointer<ViewModelBase> target = item;
    const bool hadTarget = item != nullptr;
    checkClose(getChildren(), [this, target, hadTarget](bool allowed) {
        if (hadTarget && !target) return;
        if (allowed) changeActiveItem(target.data(), true);
        else onActivationProcessed(target.data(), false);
    });
}

void ConductorSingleItem::deactivateItem(ViewModelBase *item, bool close)
{
    if (!item || item != activeItem()) return;
    QPointer<ViewModelBase> target = item;
    checkClose({item}, [this, target, close](bool allowed) {
        if (allowed && target) changeActiveItem(nullptr, close);
    });
}

void ConductorSingleItem::onActivate()
{
    ScreenExtensions::tryActivate(activeItem());
}

void ConductorSingleItem::onDeactivate(bool close)
{
    ScreenExtensions::tryDeactivate(activeItem(), close);
}

void ConductorSingleItem::canClose(CloseCallback callback)
{
    checkClose(getChildren(), std::move(callback));
}
