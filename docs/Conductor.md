# Conductor：CM 3.2.0 基类与活动项行为

本轮以 CM 3.2.0 源码为依据统一单项和 Collection.OneActive 的分层、切换顺序、父级关闭及扩展点。历史验收记录保留当时结果，当前行为以本文为准。

## 基类结构

```text
ScreenViewModel
└── ConductorBase
    └── ConductorBaseWithActiveItem
        ├── ConductorSingleItem → Conductor<T>
        └── ConductorCollectionOneActiveBase → ConductorCollectionOneActive<T>
```

| Qt 类型 | CM 3.2.0 对应职责 |
| --- | --- |
| ConductorBase | ConductorBase<T>：关闭策略、抽象管理接口、activationProcessed 和 ensureItem。 |
| ConductorBaseWithActiveItem | ConductorBaseWithActiveItem<T>：活动项属性及公共 changeActiveItem。 |
| ConductorSingleItem + Conductor<T> | Conductor<T>：单项替换、停用、守卫和父级生命周期。 |
| ConductorCollectionOneActiveBase + ConductorCollectionOneActive<T> | Conductor<T>.Collection.OneActive：成员、相邻项和集合关闭。 |

Qt 元对象必须放在非模板层；模板层提供类型约束和类型化 C++ 入口。T 为非 const/volatile 的 QObject 派生类型，默认 T = QObject。activeItem 为 QObject*，getChildren 与关闭策略使用 QList<QObject*>，activationProcessed 的对象参数也是 QObject*。类型化模板仍提供 T* 入口，基类、IConductor 和 QML 属性入口均检查实际类型，拒绝不匹配对象。两个具体类继承同一个 activeItem 属性和 activeItemChanged 信号，不再分别保存活动项。

## 名称迁移

| 原名称 | 当前名称 |
| --- | --- |
| `ConductorViewModelBase` | `ConductorSingleItem` |
| `ConductorCollectionOneActiveViewModelBase` | `ConductorCollectionOneActiveBase` |

`SingleItem` 表达只管理当前项的策略；集合实现保留 `Base` 后缀，以区别模板类 `ConductorCollectionOneActive<T>`。对应头文件、源文件和 QML 注册类型同步改名，不保留旧头文件、C++ 别名或 QML 类型别名。两个新 QML 类型继续不可直接创建。

直接使用旧实现类的消费工程须迁移名称及包含路径并重新编译。`ConductorBase`、`ConductorBaseWithActiveItem`、`Conductor<T>` 和 `ConductorCollectionOneActive<T>` 名称及模板调用形式不变。本次改名保留已有生命周期、关闭策略和所有权行为；历史验收记录继续使用当时的名称。

## 公共基类

ConductorBase 的 getChildren、activateItem、deactivateItem 为抽象方法。canClose 的聚合由具体单项和集合实现；自定义基类派生对象默认沿用 Screen 的立即同意。

closeStrategy 首次读取时创建默认策略；setCloseStrategy(nullptr) 清空缓存，下一次读取重新创建。onActivationProcessed 和 ensureItem 为受保护虚方法。默认 ensureItem 为实现 IChild 的新项设置逻辑 Parent，已经指向本对象时不重复设置。

ConductorBaseWithActiveItem 提供可读写 activeItem；setActiveItem 委托虚 activateItem，仍经过具体策略和守卫，不直接写成员。QML 属性可赋为已由本 Conductor 接管的对象或 null；所有权接管继续走 C++ unique_ptr。

changeActiveItem(item, closePrevious) 按原版顺序执行：

1. tryDeactivate(旧 ActiveItem, closePrevious)。
2. item = ensureItem(item)。
3. 父级活动时 tryActivate(item)。
4. 提交新 ActiveItem，发送 activeItemChanged。
5. 发送非空项的 activationProcessed(item, true)。

旧项停用、新项准备和激活钩子执行时，ActiveItem 仍为旧项。任何前置步骤抛异常都不提交新选择，也不回滚已完成的生命周期或 Parent 变化；例如新 Screen 激活钩子抛异常后它的 isActive 可能已为 true，但 ActiveItem 仍是旧项。通知不做同值过滤；非空同项请求由具体 ActivateItem 的分支处理。

## 接口分派

普通 QObject 无须继承 ViewModelBase 或 Screen；未实现 IChild 时不设置逻辑 Parent，但接管后仍由 QObject 父树持有。

两个 Conductor 统一使用 ScreenExtensions::tryActivate(QObject *) 和 tryDeactivate(QObject *, bool)，分别查询 IActivate、IDeactivate。空对象或没有对应接口时跳过；辅助函数不判断活动状态，不补初始化，不捕获异常。具体对象负责状态与幂等性，Screen 保留原实现。方法和 Qt 信号的契约见 [Screen](ScreenViewModel.md)。

## 单项 Conductor

新项接管和当前项停用均先询问关闭策略；拒绝时保留当前选择。activateItem 的非空同项分支在父活动时再次调用激活接口并公布成功，父非活动时无操作。空项请求仍进入策略与公共切换流程。

切换新项传 closePrevious=true；旧项关闭后保留逻辑 Parent。deactivateItem(current, false) 普通停用并清空选择，旧项仍可恢复；closeItem(current) 关闭并清空选择。非当前项的停用/关闭请求无操作。getChildren 返回只含当前项的快照；没有当前项时包含一个空指针，与 CM 的单元素枚举对应。

父 activate 激活当前项；父 deactivate(false/true) 只传播对应动作，保留 ActiveItem 和 Parent。父初始化不初始化子项；未初始化父关闭沿用 Screen 的无操作。重复父关闭会再次调用已初始化当前 Screen 的关闭钩子。

## Collection.OneActive

addItem(unique_ptr) 只添加成员和设置 Parent。选择新对象时先停用旧项，再由 ensureItem 添加新成员并设置 Parent，然后激活及提交选择。成员和选择通知是不同阶段，itemsChanged 观察者可能仍看到旧 ActiveItem。

ensureItem(nullptr) 调用 determineNextItemToActivate：优先前一项，关闭/选择首项时若存在第二项则选第二项，否则为空。因此 activateItem(nullptr) 会尝试选择相邻项，不保证清空选择。determineNextItemToActivate 为受保护虚方法，应用可覆盖选择规则。

普通 deactivateItem(item, false) 不询问守卫，不修改选择。关闭成员先询问守卫和 prepareCloseItem；当前项先执行公共切换及结果通知，再移除旧成员并清空其 Parent；非当前项先关闭再移除。关闭钩子执行时旧 Parent 和成员记录仍在。

父普通停用只停用当前项。父关闭先关闭全部成员，再清空 Items；保留 ActiveItem 引用，不发送 activeItemChanged。与原版集合 Reset 一致，此路径不逐项执行 Remove 的 Parent 清理。重复父关闭时集合已空，不再重复关闭原成员。恢复激活默认只激活保留的 ActiveItem，不会自动重新加入 Items。

默认 canClose 检查全部成员且不部分关闭。DefaultCloseStrategy(true) 或自定义 executeWithResult 可返回允许部分关闭的成员；整体拒绝时，OneActive 关闭并移除这些成员，必要时跳过它们选择未关闭的相邻项，最后交付 false。见 [关闭守卫](关闭守卫.md)。

## Qt 所有权与适配边界

unique_ptr 接管仍校验父对象、线程、自身/祖先、IActivate 活动状态和逻辑 Parent。基础拒绝不移动所有权；单项通过校验后由守卫回调持有候选，许可拒绝或回调释放时回收。接管成功使用 QObject 父树与 CppOwnership。裸指针只接受本 Conductor 已拥有的对象，这是 C++ 内存管理边界。

关闭生命周期和移出集合不再自动 deleteLater。已接管对象保留到所有者销毁，调用方也可在生命周期及 View 卸载完成后显式释放；Qt 不提供 .NET GC，长期存活的 Conductor 需要由应用明确安排不再使用对象的回收。意外销毁当前项时公共基类清空 QPointer 并通知；集合成员销毁同时移除记录，不自动导航。

prepareCloseItem 是本项目为 Shell 保留的业务准备钩子，不属于 CM 原版。Shell 在父关闭后恢复时，检测 ActiveItem 是否仍为集合成员；失效选择则选新建 Home，保留计数服务。普通切换仍保留 Home VM、重建 View。

所有调用在主线程执行；同步钩子和通知不得重入修改或销毁参与对象。守卫等待期间不支持重叠请求或修改参与成员。QPointer 检查只用于内存安全，不提供通用取消协议。

本次 QObject 类型迁移改变 C++ ABI 和公开虚方法签名，消费工程须同步修改并重新编译。IParent IID 为 /2.0，IConductor IID 为 /3.0；自定义 Conductor 的 ensureItem、prepareCloseItem、determineNextItemToActivate 等扩展点同步使用 QObject。详见 [接口化迁移](接口化迁移.md)。

## 源码与历史记录

依据 [ConductorBase](https://github.com/Caliburn-Micro/Caliburn.Micro/blob/3.2.0/src/Caliburn.Micro/ConductorBase.cs)、[ConductorBaseWithActiveItem](https://github.com/Caliburn-Micro/Caliburn.Micro/blob/3.2.0/src/Caliburn.Micro/ConductorBaseWithActiveItem.cs)、[单项 Conductor](https://github.com/Caliburn-Micro/Caliburn.Micro/blob/3.2.0/src/Caliburn.Micro/Conductor.cs)、[OneActive](https://github.com/Caliburn-Micro/Caliburn.Micro/blob/3.2.0/src/Caliburn.Micro/ConductorWithCollectionOneActive.cs)。

历史证据：[Parent 体系验收](验收/Parent体系验收记录.md)、[CM 关闭守卫精简验收](验收/CM关闭守卫精简验收记录.md)。
