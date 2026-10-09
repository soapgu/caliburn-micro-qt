# ScreenViewModel：同步生命周期

第三批实现 ScreenViewModel，继承 ViewModelBase，在 Caliburn.Micro.Qt 1.0 注册为不可创建类型。C++ 装配层创建对象，QML 读取状态；生命周期方法没有 Q_INVOKABLE 或槽声明，QML 不能直接调用。

## 接口与状态

```cpp
bool isInitialized() const;
bool isActive() const;
void initialize();
void activate();
void deactivate(bool close = false);
bool tryClose();
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

初始化一生只执行一次。Screen 基类关闭不删除、不重置初始化或业务字段；关闭后可重新激活。初始化通知最多一次，活动通知只在 bool 改变时发送。停用条件与 [CM 3.2 Screen](https://github.com/Caliburn-Micro/Caliburn.Micro/blob/3.2.0/src/Caliburn.Micro/Screen.cs) 一致：`isActive || (isInitialized && close)`；不维护额外关闭标记或待清理资源扩展。未初始化的 Conductor 同样跳过关闭生命周期，保留当前项及父树所有权；已初始化的 Conductor 在关闭钩子中清空选择与全部持有项并延迟回收。

状态提交顺序同样对齐 CM 3.2.0：初始化、激活和停用均先提交状态，再调用对应钩子；Qt 适配保留值实际改变才通知的行为。状态通知只表示属性已改变，不表示钩子或子对象转换已经完成。生命周期钩子抛异常时原样传播，已提交状态不回滚；例如激活钩子失败后 isActive 仍为 true，后续重复 activate 不自动重试，需先 deactivate 再 activate。初始化钩子失败后 isInitialized 仍为 true，不重复执行初始化钩子。

例：activate → deactivate(false) → deactivate(true) → deactivate(true) → activate → deactivate(true)，钩子依次是初始化、激活、停用、关闭、关闭、激活、关闭；重复关闭仍执行钩子。

## 线程与重入

展示 VM 沿用项目 GUI 线程约定：对象位于应用主线程，由调用方保证生命周期在该线程同步执行。Screen 不逐次检查线程，不自动调度，也不维护转换或关闭执行标记。业务钩子及同步状态通知回调不得重入改变自身生命周期或 Conductor 管理状态；框架不提供通用拒绝、警告、排队或自动重试机制。

首次 onInitialize 中初始化 true、活动 false；onActivate 中初始化和活动均为 true；onDeactivate 中活动为 false。钩子负责业务初始化、激活和清理，不主动激活、停用或关闭自身，也不回调父对象驱动自身生命周期。递归关闭自身违反调用契约；由于已初始化对象允许重复关闭，提前提交非活动状态不能保证这种递归自行终止。钩子及同步信号观察者必须保持对象有效，不能在调用栈内销毁它。

生命周期钩子没有失败返回值，当前没有异步钩子、关闭守卫或异常恢复协议，钩子应正常返回。析构不补关闭，Screen 基类不自动传播子对象生命周期。示例 Shell 通过 Conductor 基类管理 Home；所有权和业务生命周期分别安排。

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

ScreenViewModel 实现 IChild 并声明 Q_INTERFACES(IChild)。C++ getter 为 `QObject *parentViewModel() const`，QML 同名属性只读，使用 parentViewModelChanged 通知；内部 QPointer 保存弱引用，只有实际改变才通知。受保护接口 setter 授权 ConductorBase 调用，Screen 的具体实现为私有，业务不能公开赋值。

逻辑 Parent 与 QObject::parent() 分开：构造函数的 QObject 父对象不会自动建立逻辑 Parent。Conductor 接管时建立两者；普通停用、留存和集合切换保留 Parent，显式关闭和父关闭清理时先清空 Parent，再执行关闭钩子。直接调用 Screen.deactivate(true) 只执行生命周期，不清理 Conductor 关系、不删除对象；受管项的关闭应通过 Conductor。

根 Shell 的逻辑 Parent 为空，Home/Detail 为 Shell，嵌套 Conductor 向上指向其管理者；CounterService 不属于该逻辑树。普通 ViewModelBase 默认不实现 IChild。

单项 `deactivateItem(current, false)` 清空选择并留存 VM；父 `deactivate(false)` 保留选择。已初始化单项父关闭清理当前与所有留存项。集合型子项普通停用保留选择。这些 C++ 管理入口不向 QML 声明可调用接口。完整契约见 [Conductor](Conductor.md)，结果见 [Parent 体系验收记录](Parent体系验收记录.md)。

## tryClose：受管页面请求关闭自己

`bool tryClose()` 是普通 C++ 方法，不声明 Q_INVOKABLE。每次读取当前逻辑 parentViewModel，使用 qobject_cast<IConductor *> 获取管理者并调用 deactivateItem(this, true)。不缓存管理者、不使用 QObject 父对象兜底，不直接调用自身 deactivate 或 deleteLater。

| 情况 | 结果 |
| --- | --- |
| 没有逻辑 Parent，或 Parent 不实现 IConductor | 返回 false，不改变生命周期、关系或对象寿命。 |
| 管理者拒绝目标 | 返回管理者的 false，不另行关闭。 |
| 管理者接受关闭 | 返回 true；已有 Conductor 移除记录、清空 Parent、执行关闭并安排延迟回收。 |
| 管理者抛异常 | C++ 异常原样传播，不进行第二次关闭或生命周期回滚。 |

tryClose 不要求 Screen 已活动，单项留存项也可请求关闭；是否仍受管理由 Conductor 判断。true 表示关闭请求已处理，不表示对象已经同步销毁。关闭后 Parent 为空，再次调用返回 false。根 Shell 没有逻辑 Parent，因此返回 false，不关闭窗口；根请求继续待设计。

Detail 对 QML 暴露自己的 goBack：非活动时返回 false，活动时调用 tryClose，在这个业务调用边界记录异常并返回 false。框架仍保持主线程同步、转换非重入和生命周期钩子正常返回约定；异常捕获不代表有部分转换回滚能力。完整调用链见 [应用装配](IoC与应用装配.md#detail-自关闭返回)，实际结果见 [tryClose 验收记录](tryClose验收记录.md)。

## 第五批规划与当前接口边界

5A 已实现 IWindowManager / WindowManager 通用模态弹窗及 Home 重置确认，见 [WindowManager](WindowManager.md)；5B 的 Detail 退出确认和 Conductor 关闭守卫仍为规划中、未实施、未验证，详见 [阶段划分](迭代实现计划.md#第五批阶段划分)。

5B 未来按 CM 3.2 将 tryClose 等入口迁移为普通命名的 void 请求方法；当前同步 bool 接口保持现状。Detail 沿 tryClose → Parent.closeItem → deactivateItem → 关闭策略请求关闭，IGuardClose.canClose(callback) 立即或延后回传许可；守卫消费 5A showDialogAsync 的窗口 Future 后回调，这是窗口服务的 Qt 适配方案，不把 Future 作为 Conductor 请求接口。初始化、激活、停用及关闭生命周期仍同步执行，方法返回不保证关闭已经完成，调用方需按完成通知接续处理。activationProcessed 可用于激活结果，不能代表全部关闭结果。

这些是规划职责，完整函数签名、回调类型、关闭完成通知、重载和所有权细节留待实施前设计，不能保留绕过守卫的旧请求路径。根 Shell.tryClose 与主窗口退出衔接继续留待后续，单项与集合型的守卫区别见 [Conductor 规划](Conductor.md#5b-关闭守卫与回调式请求规划)。
