#pragma once

#include <CaliburnMicroQt/ConductorBase.h>
#include <QPointer>

// 对齐 CM 的公共活动项基类；Qt 元对象放在非模板层。
class ConductorBaseWithActiveItem : public ConductorBase
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Abstract active-item conductor cannot be created directly")
    Q_PROPERTY(QObject *activeItem READ activeItem WRITE setActiveItem NOTIFY activeItemChanged)

public:
    explicit ConductorBaseWithActiveItem(QObject *parent = nullptr);
    ~ConductorBaseWithActiveItem() override;
    QObject *activeItem() const { return m_activeItem.data(); }
    // 与 CM ActiveItem setter 一致，赋值仍经过具体 Conductor 的激活及守卫流程。
    void setActiveItem(QObject *item) { activateItem(item); }

signals:
    void activeItemChanged();

protected:
    // 只提交选择，不执行生命周期、不发送通知，供批量关闭原子清空状态。
    void setActiveItemCore(QObject *item);
    virtual void changeActiveItem(QObject *item, bool closePrevious);

private:
    QPointer<QObject> m_activeItem;
    QMetaObject::Connection m_currentDestroyed;
};
