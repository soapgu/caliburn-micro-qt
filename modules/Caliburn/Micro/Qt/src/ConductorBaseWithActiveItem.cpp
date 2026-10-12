#include "FrameworkLog.h"
#include <CaliburnMicroQt/ConductorBaseWithActiveItem.h>
#include <CaliburnMicroQt/ScreenExtensions.h>

ConductorBaseWithActiveItem::ConductorBaseWithActiveItem(QObject *parent)
    : ConductorBase(parent) {}

ConductorBaseWithActiveItem::~ConductorBaseWithActiveItem()
{
    QObject::disconnect(m_currentDestroyed);
}

void ConductorBaseWithActiveItem::changeActiveItem(ViewModelBase *item, bool closePrevious)
{
    const auto identity = FrameworkLog::identity(this);
    const auto previous = FrameworkLog::identity(activeItem());
    qCDebug(FrameworkLog::conductor).noquote() << "Changing active item:" << identity << "from:" << previous
                                            << "requested:" << FrameworkLog::identity(item) << "close previous:" << closePrevious;
    // 与 CM 一致：钩子执行时 ActiveItem 仍为旧项；异常不提交新选择。
    ScreenExtensions::tryDeactivate(activeItem(), closePrevious);
    item = ensureItem(item);
    const bool selectionChanged = activeItem() != item;
    if (isActive())
        ScreenExtensions::tryActivate(item);

    QObject::disconnect(m_currentDestroyed);
    m_activeItem = item;
    if (item) {
        m_currentDestroyed = connect(item, &QObject::destroyed, this, [this] {
            m_activeItem.clear();
            emit activeItemChanged();
        });
    }
    if (selectionChanged)
        qCInfo(FrameworkLog::conductor).noquote() << "Active item changed:" << identity << "from:" << previous
                                               << "to:" << FrameworkLog::identity(item);
    emit activeItemChanged();
    onActivationProcessed(item, true);
}
