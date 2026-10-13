#pragma once

#include <CaliburnMicroQt/ConductorSingleItem.h>
#include <CaliburnMicroQt/ConductorCollectionOneActive.h>
#include <cstddef>
#include <type_traits>
#include <utility>

// 模板层只提供 C++ 类型约束；属性和信号由非模板基类提供。
template<class T = QObject>
class Conductor : public ConductorSingleItem
{
    static_assert(std::is_base_of_v<QObject, T>
                  && !std::is_const_v<T> && !std::is_volatile_v<T>,
                  "Conductor<T> 的 T 必须为非 const/volatile 的 QObject 派生类");

public:
    struct Collection { using OneActive = ConductorCollectionOneActive<T>; };
    using ConductorSingleItem::ConductorSingleItem;
    T *activeItem() const
    {
        return static_cast<T *>(ConductorSingleItem::activeItem());
    }

    template<class U, std::enable_if_t<std::is_base_of_v<T, U>
                                     && !std::is_const_v<U> && !std::is_volatile_v<U>, int> = 0>
    void activateItem(std::unique_ptr<U> &&item)
    {
        if (!validateItemChange(item.get())) {
            onActivationProcessed(item.get(), false);
            return;
        }
        adoptAndActivate(std::unique_ptr<QObject>(std::move(item)));
    }

    void activateItem(std::nullptr_t) { ConductorSingleItem::activateItem(nullptr); }
    template<class U, std::enable_if_t<std::is_convertible_v<U *, T *>, int> = 0>
    void activateItem(U *item) { ConductorSingleItem::activateItem(item); }
    void setActiveItem(T *item) { ConductorBaseWithActiveItem::setActiveItem(item); }
    template<class U, std::enable_if_t<std::is_convertible_v<U *, T *>, int> = 0>
    void deactivateItem(U *item, bool close) { ConductorSingleItem::deactivateItem(item, close); }
    void deactivateItem(std::nullptr_t, bool) {}
    void closeItem(T *item) { static_cast<IConductor *>(this)->deactivateItem(item, true); }

protected:
    bool acceptsItem(QObject *item) const override
    {
        return !item || dynamic_cast<T *>(item) != nullptr;
    }
};
