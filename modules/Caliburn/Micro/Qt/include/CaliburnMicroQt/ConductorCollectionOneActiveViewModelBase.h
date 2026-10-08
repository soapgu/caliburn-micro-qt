#pragma once

#include <CaliburnMicroQt/ScreenViewModel.h>
#include <QHash>
#include <QList>
#include <QPointer>
#include <QVariantList>
#include <memory>

// 集合只读；所有权变更通过 C++ 接口完成。
class ConductorCollectionOneActiveViewModelBase : public ScreenViewModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("集合型 Conductor 由 C++ 应用装配层创建")
    Q_PROPERTY(ViewModelBase *activeItem READ activeItem NOTIFY activeItemChanged)
    Q_PROPERTY(QVariantList items READ items NOTIFY itemsChanged)

public:
    explicit ConductorCollectionOneActiveViewModelBase(QObject *parent = nullptr);
    ~ConductorCollectionOneActiveViewModelBase() override;
    ViewModelBase *activeItem() const { return m_activeItem.data(); }
    QVariantList items() const;

signals:
    void itemsChanged();
    void activeItemChanged();

protected:
    const QList<ViewModelBase *> &itemPointers() const { return m_items; }
    bool validateItemChange(ViewModelBase *item) const;
    void adoptItem(std::unique_ptr<ViewModelBase> item, bool select);
    bool selectItem(ViewModelBase *item);
    bool closeMember(ViewModelBase *item);
    void onActivate() override;
    void onDeactivate(bool close) override;

private:
    void setSelection(ViewModelBase *item);
    void memberDestroyed(ViewModelBase *identity);
    QList<ViewModelBase *> m_items;
    QHash<ViewModelBase *, QMetaObject::Connection> m_destroyed;
    QPointer<ViewModelBase> m_activeItem;
    // destroyed 回调到达时 QPointer 已清空，保留身份仅用于判断是否为当前项。
    ViewModelBase *m_activeIdentity = nullptr;
};
