#pragma once

#include <CaliburnMicroQt/ConductorBase.h>
#include <QHash>
#include <QPointer>
#include <memory>

class ConductorViewModelBase : public ConductorBase
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Conductor 由 C++ 应用装配层创建")
    Q_PROPERTY(ViewModelBase *activeItem READ activeItem NOTIFY activeItemChanged)

public:
    explicit ConductorViewModelBase(QObject *parent = nullptr);
    ~ConductorViewModelBase() override;
    ViewModelBase *activeItem() const { return m_activeItem.data(); }
    QList<ViewModelBase *> getChildren() const override;
    bool activateItem(ViewModelBase *item) override;
    bool deactivateItem(ViewModelBase *item, bool close) override;

signals:
    void activeItemChanged();

protected:
    bool validateItemChange(ViewModelBase *item) const;
    void changeActiveItem(std::unique_ptr<ViewModelBase> item);
    void onActivate() override;
    void onDeactivate(bool close) override;

private:
    void selectOwnedItem(ViewModelBase *item);
    void closeOwnedItem(ViewModelBase *item);
    void forgetItem(ViewModelBase *item);
    void memberDestroyed(ViewModelBase *identity);
    QPointer<ViewModelBase> m_activeItem;
    ViewModelBase *m_activeIdentity = nullptr;
    // 包含当前项和普通停用后的留存项，不包含等待延迟删除的项。
    QList<ViewModelBase *> m_ownedItems;
    QHash<ViewModelBase *, QMetaObject::Connection> m_destroyed;
};
