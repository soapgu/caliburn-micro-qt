#pragma once

#include <CaliburnMicroQt/ConductorBaseWithActiveItem.h>
#include <QHash>
#include <QList>
#include <QPointer>
#include <QVariantList>
#include <memory>

// 集合只读；所有权变更通过 C++ 接口完成。
class ConductorCollectionOneActiveBase : public ConductorBaseWithActiveItem
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Collection conductor is created by the C++ composition layer")
    Q_PROPERTY(QVariantList items READ items NOTIFY itemsChanged)

public:
    explicit ConductorCollectionOneActiveBase(QObject *parent = nullptr);
    ~ConductorCollectionOneActiveBase() override;
    QVariantList items() const;
    QList<ViewModelBase *> getChildren() const override { return m_items; }
    void activateItem(ViewModelBase *item) override;
    void deactivateItem(ViewModelBase *item, bool close) override;
    void canClose(CloseCallback callback) override;

signals:
    void itemsChanged();

protected:
    const QList<ViewModelBase *> &itemPointers() const { return m_items; }
    bool validateItemChange(ViewModelBase *item) const;
    void adoptItem(std::unique_ptr<ViewModelBase> item, bool select);
    bool closeMember(ViewModelBase *item);
    // 许可通过后的业务准备；失败时不移除目标。
    virtual bool prepareCloseItem(ViewModelBase *item) { Q_UNUSED(item); return true; }
    void onActivate() override;
    void onDeactivate(bool close) override;
    ViewModelBase *ensureItem(ViewModelBase *item) override;
    virtual ViewModelBase *determineNextItemToActivate(const QList<ViewModelBase *> &items, int lastIndex);

private:
    void addMember(ViewModelBase *item);
    void removeMember(ViewModelBase *item);
    void memberDestroyed(ViewModelBase *identity);
    QList<ViewModelBase *> m_items;
    QHash<ViewModelBase *, QMetaObject::Connection> m_destroyed;
};
