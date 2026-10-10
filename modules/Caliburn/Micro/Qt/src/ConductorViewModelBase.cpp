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
    QObject::disconnect(m_currentDestroyed);
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
        selectItem(next, true);
    });
}

void ConductorViewModelBase::activateItem(ViewModelBase *item)
{
    // 旧对象可由业务再次选择；从未接管的新对象必须走 unique_ptr 入口。
    if (item && (item->parent() != this
        || (qobject_cast<IChild *>(item) && qobject_cast<IChild *>(item)->parentViewModel() != this))) {
        onActivationProcessed(item, false);
        return;
    }
    if (item == m_activeItem.data()) { selectItem(item, true); return; }
    QPointer<ViewModelBase> target = item;
    const bool hadTarget = item != nullptr;
    checkClose(getChildren(), [this, target, hadTarget](bool allowed) {
        if (hadTarget && !target) return;
        if (allowed) selectItem(target.data(), true);
        else onActivationProcessed(target.data(), false);
    });
}

void ConductorViewModelBase::selectItem(ViewModelBase *item, bool closePrevious)
{
    auto *previous = m_activeItem.data();
    if (previous == item) {
        if (isActive()) {
            activateScreen(item);
            onActivationProcessed(item, true);
        }
        return;
    }
    QObject::disconnect(m_currentDestroyed);
    m_activeItem = item;
    if (item) {
        m_currentDestroyed = connect(item, &QObject::destroyed, this, [this] {
            m_activeItem.clear();
            emit activeItemChanged();
        });
    }
    if (closePrevious) setLogicalParent(previous, nullptr);
    setLogicalParent(item, this);
    emit activeItemChanged();
    deactivateScreen(previous, closePrevious);
    if (isActive()) activateScreen(item);
    if (previous && closePrevious) previous->deleteLater();
    onActivationProcessed(item, true);
}

void ConductorViewModelBase::deactivateItem(ViewModelBase *item, bool close)
{
    if (!item || item != m_activeItem.data()) return;
    QPointer<ViewModelBase> target = item;
    checkClose({item}, [this, target, close](bool allowed) {
        if (allowed && target) selectItem(nullptr, close);
    });
}

void ConductorViewModelBase::onActivate()
{
    activateScreen(m_activeItem.data());
}

void ConductorViewModelBase::onDeactivate(bool close)
{
    if (close) selectItem(nullptr, true);
    else deactivateScreen(m_activeItem.data(), false);
}
