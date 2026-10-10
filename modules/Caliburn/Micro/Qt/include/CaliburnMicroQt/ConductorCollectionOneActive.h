#pragma once

#include <CaliburnMicroQt/ConductorCollectionOneActiveViewModelBase.h>
#include <cstddef>
#include <type_traits>
#include <utility>

template<class T = ViewModelBase>
class ConductorCollectionOneActive : public ConductorCollectionOneActiveViewModelBase
{
    static_assert(std::is_base_of_v<ViewModelBase, T>
                  && !std::is_const_v<T> && !std::is_volatile_v<T>,
                  "Collection.OneActive 的 T 必须为非 const/volatile 的 ViewModelBase 派生类");
public:
    using ConductorCollectionOneActiveViewModelBase::ConductorCollectionOneActiveViewModelBase;
    T *activeItem() const
    {
        return static_cast<T *>(ConductorCollectionOneActiveViewModelBase::activeItem());
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
        adoptItem(std::unique_ptr<ViewModelBase>(std::move(item)), false);
        return true;
    }
    template<class U, std::enable_if_t<std::is_base_of_v<T, U>
                                     && !std::is_const_v<U> && !std::is_volatile_v<U>, int> = 0>
    void activateItem(std::unique_ptr<U> &&item)
    {
        if (!item) { ConductorCollectionOneActiveViewModelBase::activateItem(nullptr); return; }
        if (!validateItemChange(item.get())) {
            onActivationProcessed(item.get(), false);
            return;
        }
        adoptItem(std::unique_ptr<ViewModelBase>(std::move(item)), true);
    }
    void activateItem(T *item) { ConductorCollectionOneActiveViewModelBase::activateItem(item); }
    void activateItem(std::nullptr_t) { ConductorCollectionOneActiveViewModelBase::activateItem(nullptr); }
    void deactivateItem(T *item, bool close) { ConductorCollectionOneActiveViewModelBase::deactivateItem(item, close); }
    void closeItem(T *item) { static_cast<IConductor *>(this)->deactivateItem(item, true); }
};
