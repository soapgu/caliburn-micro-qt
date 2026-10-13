#pragma once

#include <CaliburnMicroQt/ConductorCollectionOneActiveBase.h>
#include <cstddef>
#include <type_traits>
#include <utility>

template<class T = QObject>
class ConductorCollectionOneActive : public ConductorCollectionOneActiveBase
{
    static_assert(std::is_base_of_v<QObject, T>
                  && !std::is_const_v<T> && !std::is_volatile_v<T>,
                  "Collection.OneActive 的 T 必须为非 const/volatile 的 QObject 派生类");
public:
    using ConductorCollectionOneActiveBase::ConductorCollectionOneActiveBase;
    T *activeItem() const
    {
        return static_cast<T *>(ConductorCollectionOneActiveBase::activeItem());
    }
    QList<T *> items() const
    {
        QList<T *> result;
        for (auto *item : itemPointers())
            result.append(static_cast<T *>(item));
        return result;
    }
    template<class U, std::enable_if_t<std::is_base_of_v<T, U>
                                     && !std::is_const_v<U> && !std::is_volatile_v<U>, int> = 0>
    [[nodiscard]] bool addItem(std::unique_ptr<U> &&item)
    {
        if (!item || !validateItemChange(item.get()))
            return false;
        adoptItem(std::unique_ptr<QObject>(std::move(item)), false);
        return true;
    }
    template<class U, std::enable_if_t<std::is_base_of_v<T, U>
                                     && !std::is_const_v<U> && !std::is_volatile_v<U>, int> = 0>
    void activateItem(std::unique_ptr<U> &&item)
    {
        if (!item) { ConductorCollectionOneActiveBase::activateItem(nullptr); return; }
        if (!validateItemChange(item.get())) {
            onActivationProcessed(item.get(), false);
            return;
        }
        adoptItem(std::unique_ptr<QObject>(std::move(item)), true);
    }
    template<class U, std::enable_if_t<std::is_convertible_v<U *, T *>, int> = 0>
    void activateItem(U *item) { ConductorCollectionOneActiveBase::activateItem(item); }
    void setActiveItem(T *item) { ConductorBaseWithActiveItem::setActiveItem(item); }
    void activateItem(std::nullptr_t) { ConductorCollectionOneActiveBase::activateItem(nullptr); }
    template<class U, std::enable_if_t<std::is_convertible_v<U *, T *>, int> = 0>
    void deactivateItem(U *item, bool close) { ConductorCollectionOneActiveBase::deactivateItem(item, close); }
    void deactivateItem(std::nullptr_t, bool) {}
    void closeItem(T *item) { static_cast<IConductor *>(this)->deactivateItem(item, true); }

protected:
    bool acceptsItem(QObject *item) const override
    {
        return !item || dynamic_cast<T *>(item) != nullptr;
    }
};
