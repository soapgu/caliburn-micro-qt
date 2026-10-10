# ScreenViewModel：同步生命周期

第三批实现 ScreenViewModel，继承 ViewModelBase，在 Caliburn.Micro.Qt 1.0 注册为不可创建类型。C++ 装配层创建对象，QML 读取状态；生命周期方法没有 Q_INVOKABLE 或槽声明，QML 不能直接调用。

## 接口与状态

```cpp
bool isInitialized() const;
bool isActive() const;
void initialize();
void activate();
void deactivate(bool close = false);
void tryClose();
void canClose(CloseCallback callback) override;
```

两个 bool 是只读 Q_PROPERTY，分别使用 isInitializedChanged、isActiveChanged 通知，初始 false。派生类通过 protected 虚函数 onInitialize()、onActivate()、onDeactivate(bool close) 扩展，默认空实现。完整声明见 [ScreenViewModel.h](../modules/Caliburn/Micro/Qt/include/CaliburnMicroQt/ScreenViewModel.h)。

| 调用及状态 | 同步行为 |
| --- | --- |
| initialize，尚未初始化 | 先设置初始化状态并通知一次，再执行初始化钩子。 |
| initialize，已初始化 | 无钩子、无通知。 |
| activate，尚未初始化 | 先完成初始化，再设置活动状态并通知，最后执行激活钩子。 |
| activate，非活动 | 先设置活动状态并通知，再执行激活钩子。 |
| activate，已活动 | 无操作。 |
| deactivate(false)，活动 | 先设非活动并通知，再执行停用钩子。 |
| deactivate(false)，非活动或未初始化 | 无操作。 |
| deactivate(true)，已初始化 | 先设非活动，再执行关闭钩子；包括已停用、已关闭或初始化后从未激活的对象，每次调用都执行钩子，活动状态实际改变才通知。 |
| deactivate(true)，未初始化 | 无操作。 |

初始化一生只执行一次。Screen 基类关闭不删除、不重置初始化或业务字段；关闭后可重新激活。初始化通知最多一次，活动通知只在 bool 改变时发送。停用条件与 [CM 3.2 Screen](https://github.com/Caliburn-Micro/Caliburn.Micro/blob/3.2.0/src/Caliburn.Micro/Screen.cs) 一致：`isActive || (isInitialized && close)`；不维护额外关闭标记或待清理资源扩展。未初始化的 Conductor 同样跳过关闭生命周期，保留当前项及父树所有权；已初始化的 Conductor 在关闭钩子中清空选择与受管项并延迟回收，单项只处理当前项，集合处理全部成员。

5B 的有效停用还会在状态提交前发送 attemptingDeactivation(close)，钩子正常结束后发送 deactivated(close)。状态提交顺序同样对齐 CM 3.2.0：初始化、激活和停用均先提交状态，再调用对应钩子；Qt 适配保留值实际改变才通知的行为。状态通知只表示属性已改变，不表示钩子或子对象转换已经完成。生命周期钩子抛异常时原样传播，已提交状态不回滚；例如激活钩子失败后 isActive 仍为 true，后续重复 activate 不自动重试，需先 deactivate 再 activate。初始化钩子失败后 isInitialized 仍为 true，不重复执行初始化钩子。

例：activate → deactivate(false) → deactivate(true) → deactivate(true) → activate → deactivate(true)，钩子依次是初始化、激活、停用、关闭、关闭、激活、关闭；重复关闭仍执行钩子。

## 线程与重入

展示 VM 沿用项目 GUI 线程约定：对象位于应用主线程，由调用方保证生命周期在该线程同步执行。Screen 不逐次检查线程，不自动调度，也不维护转换或关闭执行标记。业务钩子及同步状态通知回调不得重入改变自身生命周期或 Conductor 管理状态；框架不提供通用拒绝、警告、排队或自动重试机制。

首次 onInitialize 中初始化 true、活动 false；onActivate 中初始化和活动均为 true；onDeactivate 中活动为 false。钩子负责业务初始化、激活和清理，不主动激活、停用或关闭自身，也不回调父对象驱动自身生命周期。递归关闭自身违反调用契约；由于已初始化对象允许重复关闭，提前提交非活动状态不能保证这种递归自行终止。钩子及同步信号观察者必须保持对象有效，不能在调用栈内销毁它。

生命周期钩子没有失败返回值，没有异步钩子或异常恢复协议；5B 已新增请求层关闭守卫，钩子应正常返回。析构不补关闭，Screen 基类不自动传播子对象生命周期。示例 Shell 通过 Conductor 基类管理 Home；所有权和业务生命周期分别安排。

## 框架自动驱动生命周期

普通业务 Screen 无需在构造、QML Loaded 或生命周期钩子中调用自身 activate。Bootstrapper 在加载根 QML 前调用根 activate，自动完成根初始化；WindowManager 在装配弹窗 View 前激活弹窗；活动 Conductor 在选择子项时自动初始化并激活它，尚未活动的 Conductor 则在自身激活时驱动当前子项。构造或只加载、绑定 View 本身不保证激活。ViewHost 不驱动生命周期，窗口焦点也不驱动生命周期。

业务导航选择由 Conductor 的 activateItem 入口处理；页面按钮请求关闭由 tryClose 或窗口服务处理，不在关闭钩子中再次请求关闭。

## 4C 集合导航的生命周期

Shell 继承 Conductor<ScreenViewModel>::Collection::OneActive，Home 和 Detail 均继承 Screen，计数唯一保存在 CounterService。构造选择未初始化 Home；Shell.initialize() 只初始化自身，首次 activate 才初始化并激活 Home。通知顺序为 Shell 初始化、Shell 活动、Home 初始化、Home 活动；停用时先通知 Shell 非活动，再传播子项停用。父状态通知不表示子项已完成转换。

进入 Detail 普通停用 Home，Home VM 留在集合；返回关闭并移除 Detail、重新激活原 Home，Home 不重复初始化。Home View 离开时销毁、返回时新建，不能用 View 创建次数判断 VM 初始化次数。普通父停用保留当前选择，恢复时只激活当前页。

已初始化 Shell 关闭清空全部成员及选择，关闭 Home/Detail 并延迟删除；再激活创建新 Home，服务和计数保留。未初始化 Shell 关闭仍无操作。页面意外销毁不自动导航，下次 Shell 停用后激活补 Home，在空选择时选 Home。

Screen 自身关闭不删除的契约不变；集合成员移除、延迟回收属于 Conductor。Bootstrapper 只驱动根生命周期，ViewHost 不驱动生命周期，窗口失焦不自动停用。退出时当前 View 先于对应 VM 释放，根 View 先于根 VM 释放。

基础历史结果见 [第三批验收记录](第三批验收记录.md) 和 [4B 验收记录](4B验收记录.md)。当前集合生命周期、异常恢复、单次回收及导航验收见 [4C 验收记录](4C验收记录.md)；装配接口见 [IoC 与应用装配](IoC与应用装配.md)。

## IChild 与只读逻辑 Parent

ScreenViewModel 实现 IChild 并声明 Q_INTERFACES(IChild IGuardClose)。C++ getter 为 `QObject *parentViewModel() const`，QML 同名属性只读，使用 parentViewModelChanged 通知；内部 QPointer 保存弱引用，只有实际改变才通知。受保护接口 setter 授权 ConductorBase 调用，Screen 的具体实现为私有，业务不能公开赋值。

逻辑 Parent 与 QObject::parent() 分开：构造函数的 QObject 父对象不会自动建立逻辑 Parent。Conductor 接管时建立两者；单项普通停用和集合切换保留 Parent，显式关闭和父关闭清理时先清空 Parent，再执行关闭钩子。直接调用 Screen.deactivate(true) 只执行生命周期，不清理 Conductor 关系、不删除对象；受管项的关闭应通过 Conductor。

根 Shell 的逻辑 Parent 为空，Home/Detail 为 Shell，嵌套 Conductor 向上指向其管理者；CounterService 不属于该逻辑树。普通 ViewModelBase 默认不实现 IChild。

单项 `deactivateItem(current, false)` 清空选择并留存 VM；父 `deactivate(false)` 保留选择。已初始化单项父关闭只处理当前项；普通停用后的旧对象由父树兜底回收。集合型子项普通停用保留选择。这些 C++ 管理入口不向 QML 声明可调用接口。完整契约见 [Conductor](Conductor.md)，结果见 [Parent 体系验收记录](Parent体系验收记录.md)。

## tryClose：受管页面请求关闭自己

void tryClose() 是普通 C++ 方法，不声明 Q_INVOKABLE。每次读取逻辑 Parent，转换为 IConductor 并请求 deactivateItem(this, true)；无逻辑 Parent 时发送 closeRequested()，由关联的根窗口桥接处理。存在非 Conductor 的逻辑 Parent 时无操作，不使用 QObject 父对象兜底，不直接删除自身。单项只处理当前项的请求，普通停用后的非当前旧对象请求无操作。

5B 已将请求入口迁移为 void，不带完成回调。Detail 的 Q_INVOKABLE void goBack() 仅活动时发起 tryClose；守卫拒绝时保留页面，接受后关闭返回 Home。方法返回不能表示关闭完成。

Screen 新增 attemptingDeactivation(bool close) 与 deactivated(bool close)，分别在有效生命周期执行前、状态和钩子正常完成后发送。isActiveChanged 仅表示状态变化，不代表关闭完成；拒绝许可、无效目标、未初始化关闭不伪造 deactivated。close=true 表示关闭生命周期完成，实际销毁仍可延后；重复直接 deactivate(true) 沿用既有重复关闭规则。

Screen 默认 canClose(callback) 立即同意；Conductor 聚合当前子项快照，单项仅当前项、集合为全部成员，Detail 使用窗口 Future 转接许可。同步生命周期异常原样传播且不回滚，延后请求执行异常在 Qt 边界记录。完整规则见 [关闭守卫](关闭守卫.md) 和 [Conductor](Conductor.md)，本机证据见 [5B 验收](5B验收记录.md)。

closeRequested() 仅表达关闭请求，不是许可或完成通知；没有窗口桥接时不执行关闭。根 Shell.tryClose 经窗口关闭入口询问根 canClose，根直接完成 deactivate(true) 后则反向关闭窗口。窗口成功关闭后的根生命周期由 Bootstrapper 单次执行；普通停用不关闭窗口。见 [Bootstrapper](Bootstrapper.md) 与 [根窗口验收](根窗口关闭守卫与生命周期验收记录.md)。
