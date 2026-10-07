# Conductor<T>：泛型单项导航

本轮实现普通单项 Conductor 的核心与自动测试，尚未改造 Shell/Home 示例。`Collection.OneActive` 留到下一轮；第四批页面导航、共享服务和实际窗口验收尚未完成。

## 类型与使用

`ConductorViewModelBase : ScreenViewModel` 提供非模板实现、只读 `ViewModelBase *activeItem` 属性和 `activeItemChanged` 信号，在 `Caliburn.Micro.Qt 1.0` 注册为不可创建类型。`Conductor<T>` 无 Q_OBJECT，不单独注册 QML；默认 T 为 ViewModelBase，T 必须是非 const/volatile 的 ViewModelBase 派生类。

```cpp
#include <CaliburnMicroQt/Conductor.h>

Conductor<> navigation;
navigation.activate();
auto page = std::make_unique<MyViewModel>();
if (navigation.activateItem(std::move(page))) {
    // 接管成功，page 为空；指针只用于访问，不再负责删除。
    ViewModelBase *current = navigation.activeItem();
    Q_UNUSED(current);
}
```

仅 Screen 子类执行生命周期；普通 ViewModelBase 派生类也可被接管和切换，不需要生命周期方法。`Conductor<MyViewModel>` 的 C++ activeItem() 返回 MyViewModel*，其继承的 QML 属性仍为 ViewModelBase*，适配 ViewHost；业务 View 继续接收自己的具体 VM 类型。

接口均为普通 C++ 方法，不是 Q_INVOKABLE 或槽：

| 接口 | 行为 |
| --- | --- |
| `T *activeItem() const` | 借用当前项，初始为空。 |
| `activateItem(std::unique_ptr<U>&&)` | U 派生自 T 时可用，成功接管并返回 true；空 unique_ptr 清空当前项。 |
| `activateItem(nullptr)` | 清空当前项；已经为空时成功且不通知。 |
| `closeItem(T*)` | 只关闭当前项并清空；空值、非当前项返回 false。 |

不接收裸指针作为新活动项，不提供 Items、addItem/removeItem 或页面缓存。应用使用已接管对象的借用指针，不能再为它创建 unique_ptr。

## 所有权与切换顺序

调用方负责在应用主线程创建和操作 Conductor。接管前只校验新对象与 Conductor 位于同一线程、没有既有 QObject 父对象、不是 Conductor 自身或祖先；Screen 候选项必须尚未激活。已初始化但非活动的 Screen 可以接管。

校验失败会诊断并返回 false，不移动调用方的 unique_ptr，不修改当前项。模板入口先校验原具体类型的指针，再转换 unique_ptr，避免隐式转换导致拒绝路径丢失所有权。跨线程对象被拒绝后仍须由调用方在对象所属线程安全回收。

成功时先建立 QObject 父关系并设置 CppOwnership，再释放临时 unique_ptr。Conductor 的父树负责销毁对象，QML 不拥有 VM。

切换 A → B 的顺序固定为：

1. 校验并接管 B。
2. 更新 activeItem 为 B，并同步发送 activeItemChanged。
3. A 为 Screen 时调用 `A.deactivate(true)`。
4. Conductor 已激活且 B 为 Screen 时调用 `B.activate()`，由 Screen 完成首次初始化。
5. 为 A 安排 deleteLater，保留父关系直到实际销毁。

通知回调会看到 B，但此时 B 可能尚未初始化，A 也可能仍处于活动状态。操作返回时生命周期转换已完成；旧 VM 没有被同步删除。清空与 closeItem 使用相同顺序，只是新项为空。普通 VM 省略生命周期步骤，但采用相同的通知与回收规则。

ViewHost 在属性通知中切换 View，Loader 卸载旧 View，之后延迟回收旧 VM。嵌入资源的 QML 集成测试验证旧 View 先于旧 VM 销毁。若使用自定义视图缓存，应用仍须保证 View 不超过其借用 VM 的寿命。

## 父生命周期、线程与重入

- 尚未激活或已停用的 Conductor 只记录选择，不激活子项。Conductor.initialize() 不提前初始化子项。
- 父对象激活时激活当前 Screen，首次激活才初始化；普通停用时仅停用当前项，保留选择。
- 父对象未初始化时关闭无操作：不清空当前项、不通知、不传播子生命周期，也不安排延迟删除；子项仍由 QObject 父树持有。
- 父对象已初始化时关闭，先清空选择并通知，当前项为 Screen 时执行 deactivate(true)，随后安排 deleteLater。父对象可以重新激活，但不会恢复已关闭的子项，需要重新创建页面。
- 切换、显式关闭当前项、清空选择和已初始化父对象关闭都会安排旧项延迟删除。父对象销毁时由父树回收当前项及尚未处理的延迟删除项，析构不补调用生命周期。
- 当前项在转换之外意外销毁时清空引用并通知；旧项的销毁连接在替换时断开，不会干扰新项。
- 生命周期、切换和关闭由调用方保证在应用主线程同步执行；不逐次检查调用线程、不自动调度，也不维护转换标记或拒绝重入。回调中的嵌套导航和生命周期操作留待具体需求单独处理。

Screen 的停用条件与 CM 一致：`isActive || (isInitialized && close)`；未初始化时跳过关闭，已初始化时重复关闭仍进入钩子。当前项清空后再次关闭不会重复通知或删除子项；已关闭父对象接入新项后再次关闭可以回收新项。派生类覆盖 Conductor 生命周期钩子时必须调用相应基类实现，以传播子生命周期。钩子应正常返回；钩子和同步观察者不得在转换调用栈中销毁参与对象，不提供异常恢复协议。

## 与 Caliburn.Micro 的对应与差异

参考官方 [Conductor.cs](https://github.com/Caliburn-Micro/Caliburn.Micro/blob/master/src/Caliburn.Micro.Core/Conductor.cs)、[ConductorBaseWithActiveItem.cs](https://github.com/Caliburn-Micro/Caliburn.Micro/blob/master/src/Caliburn.Micro.Core/ConductorBaseWithActiveItem.cs)、[Screen.cs](https://github.com/Caliburn-Micro/Caliburn.Micro/blob/master/src/Caliburn.Micro.Core/Screen.cs) 和 [Collection.OneActive](https://github.com/Caliburn-Micro/Caliburn.Micro/blob/master/src/Caliburn.Micro.Core/ConductorWithCollectionOneActive.cs)。本实现并非完整 API 移植。

| 方面 | CM | 本轮 Qt 约定 |
| --- | --- | --- |
| 泛型范围 | T 为引用类型，按接口尝试生命周期。 | T 派生自 ViewModelBase；Screen 子类执行生命周期。 |
| 普通 Conductor 切换 | 关闭旧项，不保留 Items 集合。 | 相同的单项与 close=true 语义。 |
| 通知顺序 | 更新 ActiveItem 并通知，再停用旧项、激活新项。 | 保持相同可观察顺序。 |
| 旧对象回收 | 不直接销毁，依赖 .NET 引用与 GC。 | 接管对象，切换或父关闭后 deleteLater；旧项不能继续复用。 |
| 未初始化父对象关闭 | 跳过关闭生命周期。 | 相同，保留当前项和所有权。 |
| 已初始化对象重复关闭 | 每次执行关闭钩子，不维护关闭标记。 | 相同；Conductor 当前项为空时不重复通知或删除。 |
| 已初始化普通 Conductor 父关闭 | 向当前项传播 close=true，保留 ActiveItem 引用。 | 清空 activeItem、传播关闭并延迟删除当前项；再次激活父对象时无当前项。 |
| 关闭守卫和异步 | CloseStrategy、异步生命周期。 | 暂不实现守卫，沿用同步 Screen。 |
| Collection.OneActive | 保留多项，仅一个活动，切换不关闭旧项。 | 下一轮实现，不与普通 Conductor 混用。 |

CM 可重复传入已有对象；本轮 Qt 接管入口使用 unique_ptr，不重复移交已经接管的对象。返回旧页面需要重新创建 VM；业务事实应保存在寿命更长的服务中。需要保持 Home 常驻或页面复用时，应使用后续集合型 Conductor。

## 验证范围

独立 conductor CTest 直接依赖框架与 Qt Test，不依赖示例模块。覆盖泛型、普通 VM、Screen、通知顺序、延迟回收、所有权拒绝、异线程对象接管拒绝和嵌套生命周期。现有 QML 测试增加不可创建类型及 activeItem → ViewHost 的绑定、替换、清空、意外销毁和释放顺序验证。

实际命令与结果见 [Conductor 核心验收记录](Conductor核心验收记录.md)。Shell/Home 原示例行为保持不变，尚未新增真实导航窗口；麒麟仍待验证。
