# ScreenViewModel：同步生命周期

第三批实现 ScreenViewModel，继承 ViewModelBase，在 Caliburn.Micro.Qt 1.0 注册为不可创建类型。C++ 装配层创建对象，QML 读取状态；生命周期方法没有 Q_INVOKABLE 或槽声明，QML 不能直接调用。

## 接口与状态

```cpp
bool isInitialized() const;
bool isActive() const;
void initialize();
void activate();
void deactivate(bool close = false);
```

两个 bool 是只读 Q_PROPERTY，分别使用 isInitializedChanged、isActiveChanged 通知，初始 false。派生类通过 protected 虚函数 onInitialize()、onActivate()、onDeactivate(bool close) 扩展，默认空实现。完整声明见 [ScreenViewModel.h](../modules/Caliburn/Micro/Qt/include/CaliburnMicroQt/ScreenViewModel.h)。

| 调用及状态 | 同步行为 |
| --- | --- |
| initialize，尚未初始化 | 初始化钩子返回后设置状态并通知一次。 |
| initialize，已初始化 | 无钩子、无通知。 |
| activate，尚未初始化 | 先完成初始化及通知，再激活；激活钩子返回后设置活动状态并通知。 |
| activate，非活动 | 激活钩子返回后设活动状态。 |
| activate，已活动 | 无操作。 |
| deactivate(false)，活动 | 停用钩子返回后设非活动并通知。 |
| deactivate(false)，非活动或未初始化 | 无操作。 |
| deactivate(true)，已初始化 | 每次调用都执行关闭钩子，包括已停用、已关闭或初始化后从未激活的对象；活动状态实际改变才通知。 |
| deactivate(true)，未初始化 | 无操作。 |

初始化一生只执行一次。Screen 基类关闭不删除、不重置初始化或业务字段；关闭后可重新激活。初始化通知最多一次，活动通知只在 bool 改变时发送。停用条件与 [CM Screen](https://github.com/Caliburn-Micro/Caliburn.Micro/blob/master/src/Caliburn.Micro.Core/Screen.cs) 一致：`isActive || (isInitialized && close)`；不维护额外关闭标记或待清理资源扩展。未初始化的 Conductor 同样跳过关闭生命周期，保留当前项及父树所有权；已初始化的 Conductor 在关闭钩子中清空当前项并延迟回收。

例：activate → deactivate(false) → deactivate(true) → deactivate(true) → activate → deactivate(true)，钩子依次是初始化、激活、停用、关闭、关闭、激活、关闭；重复关闭仍执行钩子。

## 线程与重入

展示 VM 沿用项目 GUI 线程约定：对象位于应用主线程，由调用方保证生命周期在该线程同步执行。Screen 不逐次检查线程，不自动调度，也不维护转换标记或拒绝重入；回调中的嵌套生命周期操作留待具体需求单独处理。

钩子读取的是提交新状态前的值：首次 onInitialize 中两个状态 false；onActivate 中初始化 true、活动 false；活动对象的 onDeactivate 中活动仍为 true。钩子及同步信号观察者必须保持对象有效，不能在调用栈内销毁它。

本批无异步钩子、关闭守卫、失败返回值或异常恢复协议，钩子应正常返回。析构不补关闭，Screen 基类不自动传播子对象生命周期。示例 Shell 通过 Conductor 基类管理 Home；所有权和业务生命周期分别安排。

## 4C 集合导航的生命周期

Shell 继承 Conductor<ScreenViewModel>::Collection::OneActive，Home 和 Detail 均继承 Screen，计数唯一保存在 CounterService。构造选择未初始化 Home；Shell.initialize() 只初始化自身，首次 activate 才初始化并激活 Home。通知顺序仍为 Shell 初始化、Home 初始化、Home 激活、Shell 激活。

进入 Detail 普通停用 Home，Home VM 留在集合；返回关闭并移除 Detail、重新激活原 Home，Home 不重复初始化。Home View 离开时销毁、返回时新建，不能用 View 创建次数判断 VM 初始化次数。普通父停用保留当前选择，恢复时只激活当前页。

已初始化 Shell 关闭清空全部成员及选择，关闭 Home/Detail 并延迟删除；再激活创建新 Home，服务和计数保留。未初始化 Shell 关闭仍无操作。页面意外销毁不自动导航，下次 Shell 停用后激活补 Home，在空选择时选 Home。

Screen 自身关闭不删除的契约不变；集合成员移除、延迟回收属于 Conductor。Bootstrapper 只驱动根生命周期，ViewHost 不驱动生命周期，窗口失焦不自动停用。退出时当前 View 先于对应 VM 释放，根 View 先于根 VM 释放。

基础历史结果见 [第三批验收记录](第三批验收记录.md) 和 [4B 验收记录](4B验收记录.md)。当前集合生命周期、异常恢复、单次回收及导航验收见 [4C 验收记录](4C验收记录.md)；装配接口见 [IoC 与应用装配](IoC与应用装配.md)。
