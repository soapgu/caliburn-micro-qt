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
    QList<QObject *> getChildren() const override { return m_items; }
    void activateItem(QObject *item) override;
    void deactivateItem(QObject *item, bool close) override;
    void canClose(CloseCallback callback) override;

signals:
    void itemsChanged();

protected:
    const QList<QObject *> &itemPointers() const { return m_items; }
    bool validateItemChange(QObject *item) const;
    void adoptItem(std::unique_ptr<QObject> item, bool select);
    bool closeMember(QObject *item);
    // 许可通过后的业务准备；失败时不移除目标。
    virtual bool prepareCloseItem(QObject *item) { Q_UNUSED(item); return true; }
    void onActivate() override;
    void onDeactivate(bool close) override;
    QObject *ensureItem(QObject *item) override;
    virtual QObject *determineNextItemToActivate(const QList<QObject *> &items, int lastIndex);

private:
    void addMember(QObject *item);
    void removeMember(QObject *item);
    void memberDestroyed(QObject *identity);
    QList<QObject *> m_items;
    QHash<QObject *, QMetaObject::Connection> m_destroyed;
};
