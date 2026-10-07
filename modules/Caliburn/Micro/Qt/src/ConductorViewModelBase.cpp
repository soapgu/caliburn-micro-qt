#include <CaliburnMicroQt/ConductorViewModelBase.h>
#include <QQmlEngine>

ConductorViewModelBase::ConductorViewModelBase(QObject *parent) : ScreenViewModel(parent) {}

ConductorViewModelBase::~ConductorViewModelBase()
{
    // QObject 父树随后回收子项；此时派生成员即将销毁，不再发送活动项通知。
    QObject::disconnect(m_destroyed);
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
    return true;
}

void ConductorViewModelBase::changeActiveItem(std::unique_ptr<ViewModelBase> item)
{
    auto *next = item.get();
    if (next) {
        next->setParent(this);
        QQmlEngine::setObjectOwnership(next, QQmlEngine::CppOwnership);
        item.release();
    }
    auto *previous = m_activeItem.data();
    if (previous == next)
        return;

    QObject::disconnect(m_destroyed);
    m_destroyed = {};
    m_activeItem = next;
    if (next) {
        m_destroyed = connect(next, &QObject::destroyed, this, [this] {
            // destroyed 发出时 QPointer 已清空，仍需要通知 View 卸载。
            m_activeItem.clear();
            emit activeItemChanged();
        });
    }
    // 与 CM 一致：先公布新项，再关闭旧项，最后按父状态激活新项。
    emit activeItemChanged();
    if (auto *screen = qobject_cast<ScreenViewModel *>(previous))
        screen->deactivate(true);
    if (isActive()) {
        if (auto *screen = qobject_cast<ScreenViewModel *>(next))
            screen->activate();
    }
    if (previous)
        previous->deleteLater();
}

bool ConductorViewModelBase::closeCurrentItem(ViewModelBase *item)
{
    if (!item || item != m_activeItem.data())
        return false;
    changeActiveItem(nullptr);
    return true;
}

void ConductorViewModelBase::onActivate()
{
    if (auto *screen = qobject_cast<ScreenViewModel *>(m_activeItem.data()))
        screen->activate();
}

void ConductorViewModelBase::onDeactivate(bool close)
{
    if (close) {
        changeActiveItem(nullptr);
    } else if (auto *screen = qobject_cast<ScreenViewModel *>(m_activeItem.data())) {
        screen->deactivate(false);
    }
}
