# Conductor：CM 3.2.0 基类与活动项行为

以 CM 3.2.0 源码为依据统一单项和 Collection.OneActive 的分层、切换顺序及扩展点；Qt 的所有权回收和父级关闭收尾采用本文约定。历史验收记录保留当时结果，当前行为以本文为准。

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

切换新项传 closePrevious=true；提交新选择及激活结果后，清空旧项逻辑 Parent 并进入框架回收流程。deactivateItem(current, false) 普通停用并清空选择，旧项保留并可恢复；closeItem(current) 关闭并清空选择，随后回收旧项。非当前项的停用/关闭请求无操作。getChildren 返回只含当前项的快照；没有当前项时包含一个空指针，与 CM 的单元素枚举对应。

父 activate 激活当前项；父 deactivate(false) 传播普通停用并保留选择。父 deactivate(true) 先关闭当前项，再清空选择与子项逻辑 Parent，发送一次 activeItemChanged 并安排回收；再次激活不会恢复已关闭对象。父初始化不初始化子项；未初始化父关闭沿用 Screen 的无操作。重复父关闭不重复关闭或通知已移除的当前项。

## Collection.OneActive

addItem(unique_ptr) 只添加成员和设置 Parent。选择新对象时先停用旧项，再由 ensureItem 添加新成员并设置 Parent，然后激活及提交选择。成员和选择通知是不同阶段，itemsChanged 观察者可能仍看到旧 ActiveItem。

ensureItem(nullptr) 调用 determineNextItemToActivate：优先前一项，关闭/选择首项时若存在第二项则选第二项，否则为空。因此 activateItem(nullptr) 会尝试选择相邻项，不保证清空选择。determineNextItemToActivate 为受保护虚方法，应用可覆盖选择规则。

普通 deactivateItem(item, false) 不询问守卫，不修改选择。关闭成员先询问守卫和 prepareCloseItem；当前项先执行公共切换及结果通知，再移除旧成员并清空其 Parent；非当前项先关闭再移除。关闭钩子执行时旧 Parent 和成员记录仍在。

集合始终满足 `activeItem == nullptr || items.contains(activeItem)`。父普通停用只停用当前项并保留选择与集合。父关闭指调用父 Conductor 的 deactivate(true)，不代表销毁父对象：先关闭全部成员，再一次提交空 Items 与空 ActiveItem，清空成员逻辑 Parent；有旧选择时发送一次 activeItemChanged，有旧成员时发送一次 itemsChanged，然后逐项安排回收。Parent 和集合/选择通知的观察者均看到空集合与空选择；关闭钩子仍看到原成员和 Parent。重复关闭不重复关闭成员或发送集合/选择通知。恢复激活不会恢复已关闭的成员，由应用工厂按需新建页面。

默认 canClose 检查全部成员且不部分关闭。DefaultCloseStrategy(true) 或自定义 executeWithResult 可返回允许部分关闭的成员；整体拒绝时，OneActive 关闭并移除这些成员，必要时跳过它们选择未关闭的相邻项，最后交付 false。见 [关闭守卫](关闭守卫.md)。

## CM 3.2.0 核心行为与界面联动

CM 3.2.0 的 ActiveItem getter 直接返回独立字段 activeItem，并不根据 Items 动态查找或检查成员关系。OneActive 的父级关闭路径关闭成员并调用 items.Clear()，该类的 CollectionChanged Reset 分支也没有直接清空 ActiveItem。因此，在没有界面绑定或其他回调修改选择时，仅清空 Items 不会自动清空这个字段。这个结论限定于 Conductor 自身的源码行为，不能推广成所有应用运行场景都会保留旧选择。依据 [活动项基类](https://github.com/Caliburn-Micro/Caliburn.Micro/blob/3.2.0/src/Caliburn.Micro/ConductorBaseWithActiveItem.cs#L6) 与 [OneActive](https://github.com/Caliburn-Micro/Caliburn.Micro/blob/3.2.0/src/Caliburn.Micro/ConductorWithCollectionOneActive.cs#L158)。

实际界面还可能存在选择绑定联动。CM 的约定会将 TabControl.ItemsSource 绑定到 Items，将 SelectedItem 绑定到 ActiveItem；存在双向绑定时，控件清空选择可能将 null 写回 ActiveItem。是否发生回写取决于具体控件及绑定流程，不能仅凭 items.Clear() 推断最终界面状态。本说明核查了源码与官方绑定约定，未运行 WPF 场景验证这条回写链路。见 [CM 官方组合文档](https://caliburnmicro.com/documentation/composition)。

本项目 Qt 实现直接保证集合与选择的一致性：父级关闭在核心层同时清空 items 和 activeItem，不依赖界面控件回写。这里明确的是核心层保证的差异，并非断言 CM 3.2.0 的实际界面关闭后一定残留 ActiveItem。

## Qt 所有权与适配边界

unique_ptr 接管仍校验父对象、线程、自身/祖先、IActivate 活动状态和逻辑 Parent。基础拒绝不移动所有权；单项通过校验后由守卫回调持有候选，许可拒绝或回调释放时回收。接管成功使用 QObject 父树与 CppOwnership。裸指针只接受本 Conductor 已拥有的对象，这是 C++ 内存管理边界。

Conductor 负责回收已接管且关闭退出管理的对象，包括单项替换、显式关闭、集合非当前项关闭、部分关闭和父级关闭。普通停用、集合内部切换及被守卫拒绝的关闭保留对象。独立 Screen 的 deactivate(true) 仍只执行生命周期，不自行删除。

内部 ViewModelLifetime 仅由 Conductor 接管成员时创建，跟踪 ViewHost 为这些成员创建的实际 View。回收请求驱动仍关联旧 VM 的宿主清空模型，等全部已登记 View 销毁后安排 VM deleteLater；没有 View 的对象直接安排延迟删除。存在 View 借用时，VM 暂时脱离 QObject 父树，避免 Conductor 提前析构导致 VM 先于 View 释放；管理者析构也会将仍被 View 借用的成员交给此流程。View 全部释放后，VM 交回仍存活的原所有者（或同线程应用对象）作父树兜底，退出时事件循环已停止也能回收。待回收对象不能重新选择或装配。应用不再提供 afterViewReleased 回调。自定义页面宿主应使用 ViewHostState，创建 View 后调用 retainView(view)，并在 modelChanged 后卸载旧 View；直接借用 VM 的任意外部指针不属于框架追踪范围。

根窗口 VM、当前多屏示例的辅助窗口 VM 和模态框根 VM 不建立借用登记，也不创建 ViewModelLifetime。它们的删除分别由 Bootstrapper、应用持有者及 WindowManager 的弹窗收尾路径负责，沿用先释放 View 再释放 VM 的顺序。窗口桥接不接入页面回收协调器；ViewHost 对非 Conductor 成员只提供弱引用保护和装配，不加入回收流程。

意外销毁当前项时公共基类清空 QPointer 并通知；集合成员销毁同时移除记录，不自动导航。父级关闭收尾及自动回收改变原有行为，消费工程需重新编译；应用恢复页面应使用工厂新建，不得保存已关闭 VM 的裸指针再激活。

prepareCloseItem 是本项目为 Shell 保留的业务准备钩子，不属于 CM 原版。Shell 在父关闭后恢复时，为空选择创建并选中 Home，保留计数服务。普通切换仍保留 Home VM、重建 View。

所有调用在主线程执行；同步钩子和通知不得重入修改或销毁参与对象。守卫等待期间不支持重叠请求或修改参与成员。QPointer 检查只用于内存安全，不提供通用取消协议。

本次 QObject 类型迁移改变 C++ ABI 和公开虚方法签名，消费工程须同步修改并重新编译。IParent IID 为 /2.0，IConductor IID 为 /3.0；自定义 Conductor 的 ensureItem、prepareCloseItem、determineNextItemToActivate 等扩展点同步使用 QObject。详见 [接口化迁移](接口化迁移.md)。

## 源码与历史记录

依据 [ConductorBase](https://github.com/Caliburn-Micro/Caliburn.Micro/blob/3.2.0/src/Caliburn.Micro/ConductorBase.cs)、[ConductorBaseWithActiveItem](https://github.com/Caliburn-Micro/Caliburn.Micro/blob/3.2.0/src/Caliburn.Micro/ConductorBaseWithActiveItem.cs)、[单项 Conductor](https://github.com/Caliburn-Micro/Caliburn.Micro/blob/3.2.0/src/Caliburn.Micro/Conductor.cs)、[OneActive](https://github.com/Caliburn-Micro/Caliburn.Micro/blob/3.2.0/src/Caliburn.Micro/ConductorWithCollectionOneActive.cs)。

历史证据：[Parent 体系验收](验收/Parent体系验收记录.md)、[CM 关闭守卫精简验收](验收/CM关闭守卫精简验收记录.md)。
