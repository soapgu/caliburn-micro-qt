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

本批无异步钩子、关闭守卫、失败返回值或异常恢复协议，钩子应正常返回。析构不补关闭，Screen 基类不自动传播子对象生命周期。示例 Shell 通过钩子显式管理 Home；所有权和业务生命周期分别安排。

## 示例装配与验收

Shell 和 Home 均继承 Screen，计数完整保存在 Home。IoC 递归构造 VM 树，Shell 在构造函数中接管 Home。启动顺序：buildShell 创建 VM 树并设置 CppOwnership → 登记 Shell/Home 映射 → 初始化根 Shell → 激活根 Shell → 注入 Shell → 注册表定位并加载根窗口。Shell 的 onInitialize/onActivate 显式调用 Home 对应方法，子状态在父钩子返回前完成，因此状态通知先 Home 后 Shell。

退出或根加载失败只调用 shell->deactivate(true)，由 Shell 的 onDeactivate(close) 先关闭 Home，返回后提交 Shell 状态。随后先销毁引擎与 View，再释放 VM 树。窗口失焦和 ViewHost 装配不触发生命周期。Shell 显示两者状态，不新增停用/恢复按钮。

自动测试验证钩子与通知顺序、初始化/激活/普通停用幂等、自动初始化、停用后关闭、重复关闭、重激活及关闭不删除。第三批基础结果见 [第三批验收记录](第三批验收记录.md)；IoC 调整后新增根生命周期与接管验证，见 [IoC 装配验收记录](IoC装配验收记录.md)。装配契约见 [IoC 与应用装配](IoC与应用装配.md) 和 [ViewHost](ViewHost.md)。
