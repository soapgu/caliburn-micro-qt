#pragma once

#include <CaliburnMicroQt/ConductorViewModelBase.h>
#include <cstddef>
#include <type_traits>
#include <utility>

// 模板层只提供 C++ 类型约束；属性和信号由非模板基类提供。
template<class T = ViewModelBase>
class Conductor : public ConductorViewModelBase
{
    static_assert(std::is_base_of_v<ViewModelBase, T>
                  && !std::is_const_v<T> && !std::is_volatile_v<T>,
                  "Conductor<T> 的 T 必须为非 const/volatile 的 ViewModelBase 派生类");

public:
    using ConductorViewModelBase::ConductorViewModelBase;
    T *activeItem() const
    {
        return static_cast<T *>(ConductorViewModelBase::activeItem());
    }

    template<class U, std::enable_if_t<std::is_base_of_v<T, U>
                                     && !std::is_const_v<U> && !std::is_volatile_v<U>, int> = 0>
    [[nodiscard]] bool activateItem(std::unique_ptr<U> &&item)
    {
        // 校验前不转换 unique_ptr，拒绝时调用方仍持有原来的具体类型对象。
        if (!validateItemChange(item.get()))
            return false;
        changeActiveItem(std::unique_ptr<ViewModelBase>(std::move(item)));
        return true;
    }

    [[nodiscard]] bool activateItem(std::nullptr_t)
    {
        changeActiveItem(nullptr);
        return true;
    }

    [[nodiscard]] bool closeItem(T *item) { return closeCurrentItem(item); }
};
