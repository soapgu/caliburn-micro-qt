# Conductor：单项与 Collection.OneActive

第四批 4A、4B、4C 已实现并通过本机验收。当前 Shell 使用 Collection.OneActive，Home VM 常驻、Detail 按需创建，View 每次新建；麒麟待验证。Parent 与统一协议是第四批之后的独立增量，历史结果见 [实现计划](迭代实现计划.md#第四批阶段划分)，本轮结果见 [Parent 体系验收记录](Parent体系验收记录.md)。

## 统一协议与逻辑 Parent

三个纯 C++ 接口均有虚析构，不继承 QObject，通过 Q_DECLARE_INTERFACE 声明，IID 分别为 `Caliburn.Micro.Qt.IChild/1.0`、`Caliburn.Micro.Qt.IParent/1.0`、`Caliburn.Micro.Qt.IConductor/1.0`。

| 接口 | 方法与职责 |
| --- | --- |
| IChild | `QObject *parentViewModel() const`，逻辑父级；受保护 setter 仅授权公共 Conductor 基类维护。 |
| IParent | `QList<ViewModelBase *> getChildren() const`，返回逻辑子项的借用快照。 |
| IConductor : IParent | `bool activateItem(ViewModelBase *)`、`bool deactivateItem(ViewModelBase *, bool close)`，操作已接管且尚未关闭的对象。 |

`ConductorBase : ScreenViewModel, IConductor` 是抽象元对象基类，声明 Q_INTERFACES(IParent IConductor)，提供 Parent 辅助逻辑及 activationProcessed 信号，在 QML 注册为不可创建类型。两个具体元对象基类均继承它，可通过 qobject_cast<IParent *> / qobject_cast<IConductor *> 统一访问。

Screen 实现 IChild，QML 只读 parentViewModel 使用 QPointer<QObject> 保存，实际变化才通知。普通 ViewModelBase 默认不实现 IChild；自行实现并声明 Q_INTERFACES(IChild) 的普通 VM 也会被维护 Parent。逻辑 Parent 的写入不调用 QObject::setParent()，QObject 父树独立负责所有权。

## 单项类型与使用

`ConductorViewModelBase : ConductorBase` 提供非模板算法、只读 ViewModelBase* activeItem 和 activeItemChanged，在 QML 注册为不可创建类型。`Conductor<T>` 无独立元对象，默认 T 为 ViewModelBase，T 必须是非 const/volatile 的 ViewModelBase 派生类。只有 Screen 子类执行生命周期。

```cpp
Conductor<ScreenViewModel> conductor;
conductor.activate();
auto owned = std::make_unique<MyScreen>();
auto *page = owned.get(); // 借用，成功后不能再负责删除。
if (conductor.activateItem(std::move(owned))) {
    conductor.deactivateItem(page, false); // 清空选择，VM 与 Parent 保留。
    conductor.activateItem(page);          // 恢复原 VM，不重复初始化。
    conductor.closeItem(page);             // 移除记录、清空 Parent、延迟删除。
}
```

接口是普通 C++ 方法，不是 Q_INVOKABLE 或槽。模板层提供类型约束，QML activeItem 属性仍为 ViewModelBase*。

| 接口或操作 | 选择与对象处理 |
| --- | --- |
| activateItem(unique_ptr<U>&&) | 接管并选择新项，关闭旧当前项；其他留存项不受影响。空 unique_ptr 等同 nullptr。 |
| activateItem(T*) | 仅恢复已接管、尚未关闭的对象；另有当前项时关闭该当前项。 |
| deactivateItem(current, false) | 清空选择并通知，再普通停用旧项；保留所有权、Parent 和恢复资格。 |
| deactivateItem(retained, false) | 普通停用留存项，不改变当前选择或持有记录。 |
| closeItem(T*) / deactivateItem(item, true) | 可关闭当前项或留存项；移出记录、清空 Parent、关闭并 deleteLater。 |
| activateItem(nullptr) | 仅关闭当前项；没有当前项时无操作，不清理留存项。 |
| getChildren() | 返回当前项或空列表；不枚举留存项、等待删除项。 |

内部按接管顺序记录全部尚未关闭的对象及销毁连接，不增加公开 Items 属性。getChildren() 是 CM 风格的当前项枚举，不能当作 QObject 持有清单：留存项仍可有逻辑 Parent。辅助 QObject 和等待删除对象即使 QObject 父对象相同，也没有恢复资格；资格必须通过内部记录判断。只支持恢复到原 Conductor，没有跨 Conductor 转移接口。

## 接管、Parent 与通知顺序

接管要求新对象无 QObject 父对象、与 Conductor 同线程、不是自身或祖先、Screen 尚未活动；实现 IChild 的候选还须逻辑 Parent 为空。失败返回 false，不移动调用方 unique_ptr、不改变当前选择；非空激活失败发送一次 activationProcessed(item, false)。模板先检查具体类型，再转换 unique_ptr，避免失败时丢失调用方所有权。

成功时设置 QObject 父关系及 CppOwnership，再释放临时 unique_ptr。提交持有记录和选择后更新相关逻辑 Parent，再通知成员或选择；观察者看到更新后的 Parent。

单项 A → B：接管 B，提交选择与记录，清空 A 的 Parent、设置 B 的 Parent，发送 activeItemChanged，关闭 A，父活动时激活 B，安排 A.deleteLater，最后发送 B 激活处理成功。通知时 B 可能尚未初始化，A 可能仍活动；转换返回时生命周期已完成。关闭钩子 onDeactivate(true) 看到逻辑 Parent 为空，QObject 父关系保留到回收。

单项 close=false 的例外是保留 Parent：清空选择、发送 activeItemChanged、普通停用旧项。ViewHost 随选择清空卸载 View；恢复原 VM 时新建 View。关闭路径同样先卸载旧 View、再延迟删除 VM。

## 父生命周期与留存

| 调用 | 单项 | 集合型 |
| --- | --- | --- |
| 子项 deactivateItem(item, false) | 清空当前选择，留存 VM。非当前项不改选择。 | 停用目标，保留成员和当前选择。 |
| 父 deactivate(false) | 停用当前项，保留选择和全部持有对象。 | 停用当前项，保留选择和集合。 |
| 已初始化父 deactivate(true) | 一次清空选择及全部持有记录，关闭当前和留存项，安排删除。 | 一次清空集合与选择，关闭全部成员，安排删除，不逐项导航。 |
| 未初始化父 deactivate(true) | 无操作，父树最终回收。 | 无操作，父树最终回收。 |

父 initialize 不初始化子项；父 activate 仅激活当前 Screen。单项父关闭先清记录和选择，再清空全部子项 Parent，最后通知选择并逐项关闭。无当前项但有留存项时仍会清理；关闭后的对象立即失去恢复和重复关闭资格。父可重新激活，已关闭子项需要重新创建。

留存项意外销毁只清理记录，当前项意外销毁还清空选择并通知，均不自动导航。销毁回调不访问正在析构对象的 IChild。析构断开销毁连接，不补生命周期或关系通知，由 QObject 父树兜底回收；QPointer 随目标销毁失效。

所有快照均为借用指针，操作由调用方保证在应用主线程同步执行。转换回调不得重入修改管理状态或销毁参与对象，钩子应正常返回；没有异步和异常恢复协议。派生生命周期钩子须调用对应 Conductor 基类实现。

## Collection.OneActive（4C 已实现）

`ConductorCollectionOneActiveViewModelBase : ConductorBase` 独立管理集合，不复用单项切换关闭算法。只读 activeItem 和 QVariantList items 使用 activeItemChanged/itemsChanged；模板 `ConductorCollectionOneActive<T>` 返回 T* 和 QList<T*> 快照，也可写作 `Conductor<T>::Collection::OneActive`。复制快照不会修改集合。

| 接口 | 行为 |
| --- | --- |
| addItem(unique_ptr<U>&&) | 接管并按加入顺序添加，不选择、不初始化或激活；空值失败。 |
| activateItem(unique_ptr<U>&&) | 接管、加入并选择；空 unique_ptr 等同 nullptr。 |
| activateItem(T*) | 选择已有成员，外部对象返回 false。 |
| activateItem(nullptr) | 清空选择、普通停用旧项，集合保留。 |
| deactivateItem(T*, false) | 普通停用目标，保留成员、Parent 和当前选择。 |
| closeItem(T*) / deactivateItem(T*, true) | 关闭、移除并延迟删除；关闭当前项自动选相邻项。 |
| getChildren() / items() | 按加入顺序返回全部成员的借用快照，前者元素类型为 ViewModelBase*。 |

空停用目标、外部对象及已关闭对象返回 false。普通 VM 的有效普通停用返回 true，没有 Screen 动作。切换先提交选择并通知，再普通停用旧 Screen，父活动时激活新 Screen；旧项留在集合、Parent 保持。加入并选择先提交成员与选择、设置 Parent，再依次通知 itemsChanged、activeItemChanged。

关闭当前项优先选前一项，否则后一项，没有剩余项则清空。先提交成员与选择、清空旧 Parent，依次发送 itemsChanged、activeItemChanged，再关闭旧项、按父状态激活新项，最后安排 deleteLater。关闭非当前项不改变选择。显式关闭才自动选择相邻项；成员意外销毁只移除，当前项销毁则清空选择，不自动导航。

## 激活处理结果

ConductorBase 的 `activationProcessed(ViewModelBase *, bool)` 是一次操作完成的通知，不等同于 Screen 的 isActiveChanged：

- 新项接管并选择、不同成员选择或留存项恢复完成后，对非空目标发送一次成功通知，父非活动也发送。
- 同项重复选择：父活动时调用子项 activate 并发送成功通知；父非活动时不发送。Screen 幂等规则仍有效。
- 非空激活目标被拒绝时发送一次失败通知，模板与公共接口不重复发布。
- nullptr、只添加成员、普通停用及父生命周期传播不发送。
- 集合关闭当前项并自动选择非空相邻项，转换完成后发送一次新项成功通知。

## 与 Caliburn.Micro 的对应与差异

IChild、IParent、IConductor 对齐 CM 的逻辑角色；Parent 在 Qt 对外只读，由框架维护。单项普通停用清空选择，getChildren 仅枚举当前项；集合普通停用保留选择、枚举全部成员。

Qt 新对象显式 unique_ptr 接管，QObject 父树持有，关闭后 deleteLater；CM 使用 .NET 引用和 GC。Qt 裸指针只能操作原 Conductor 持有的尚未关闭对象。单项普通停用留存可以恢复，已经关闭的对象不能恢复。Qt 已初始化父关闭清空全部持有项；不照搬 CM 保留 ActiveItem 引用的父关闭行为。

本项目沿用既定的先通知选择、再传播生命周期顺序。CM 单项普通停用还会检查关闭策略；本轮同步接口不检查关闭守卫，也没有异步生命周期或根窗口请求；受管 Screen 的同步 tryClose 已接入统一协议。后续边界见 [ToDoList](后续版本ToDoList.md)。官方源码入口：[Conductor](https://github.com/Caliburn-Micro/Caliburn.Micro/blob/master/src/Caliburn.Micro.Core/Conductor.cs)、[Collection.OneActive](https://github.com/Caliburn-Micro/Caliburn.Micro/blob/master/src/Caliburn.Micro.Core/ConductorWithCollectionOneActive.cs)。

## Shell 示例与元对象适配

4B 历史版本使用单项 Conductor，见 [4B 验收记录](4B验收记录.md)。4C Shell 使用集合型，注入 Home/Detail 工厂，home/detail 从集合查找、以 itemsChanged 通知，ViewHost 绑定 activeItem。Home/Detail 的逻辑 Parent 为 Shell，根 Shell 的逻辑 Parent 为空；服务不属于逻辑 VM 树。装配和 CounterService 寿命未改变，见 [应用装配](IoC与应用装配.md#4c-构造接口与模块边界)。

模板没有独立元对象。Shell 继续通过 Q_MOC_RUN 分支向 moc 提供集合元对象基类，C++ 编译继承泛型集合型。进入详情保留 Home VM，返回关闭 Detail 自动回 Home；Home View 每次新建。WPF CM ViewAware/ViewLocator 的 View 复用仅作为后续参考，本轮不实现缓存。

## 验证范围

独立 parent_protocol 测试覆盖接口转换、只读 Parent、自定义 IChild、单项留存恢复、Parent/通知/生命周期顺序、激活结果及嵌套 Conductor；QML 验证停用清空 View、恢复重建 View、关闭时 View 先于 VM 销毁。旧核心及示例导航、共享服务、Bootstrapper 继续回归。

历史记录保持原样：[4A 核心](Conductor核心验收记录.md)、[4B](4B验收记录.md)、[4C](4C验收记录.md)。本轮实际结果见 [Parent 体系验收记录](Parent体系验收记录.md)，麒麟仍待验证。

## Screen.tryClose 与公共协议

Screen.tryClose 是 C++ 入口，通过逻辑 Parent 转换为 IConductor 并调用 deactivateItem(this, true)，没有新增 IConductor 接口，也不绕过具体管理算法。单项可关闭当前或留存项；集合型关闭当前项时沿用前一项优先策略。Parent、成员、选择通知及延迟删除顺序保持不变。

Shell 重写公共 deactivateItem(ViewModelBase*, bool)，在关闭当前 Detail 前确保 Home 存在，再限定调用集合元对象基类。Detail 的自关闭因此仍能恢复意外缺失的 Home；其他目标、普通停用直接委托基类。该处理属于示例的返回目标策略，不加入通用集合算法。既有模板 closeItem 辅助入口保持原实现；Detail.tryClose 通过 IConductor 虚接口进入 Shell 的返回前置处理路径。

没有 Parent、接口不匹配或管理者拒绝时 tryClose 返回 false；异常在 C++ 层传播。它不提供关闭守卫、根窗口请求或异步结果，不新增关闭信号和 canTryClose 属性。Detail.goBack 作为 QML 业务入口处理异常，见 [Screen](ScreenViewModel.md#tryclose受管页面请求关闭自己) 与 [独立验收记录](tryClose验收记录.md)。
