# Conductor：单项与 Collection.OneActive

第四批 4A、4B、4C 均已实现并通过本机验收。当前 Shell 使用 Collection.OneActive，Home VM 常驻、Detail 按需创建，View 每次新建；麒麟待验证。阶段状态见 [实现计划](迭代实现计划.md#第四批阶段划分)。

## 单项类型与使用

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
| Collection.OneActive | 保留多项，仅一个活动，切换不关闭旧项。 | 4C 已实现集合型；切换保留，关闭才移除并延迟回收。 |

CM 可重复传入已有对象；Qt 新项通过 unique_ptr 接管，不能重复移交所有权。普通单项返回旧页需重建 VM；集合型允许使用成员借用指针再次选择。业务事实可由共享服务保持。

## Collection.OneActive（4C 已实现）

`ConductorCollectionOneActiveViewModelBase : ScreenViewModel` 提供独立集合实现、只读 activeItem 和 QVariantList items 属性，使用 activeItemChanged/itemsChanged，在 QML 注册为不可创建类型。模板 `ConductorCollectionOneActive<T>` 提供类型约束、T* activeItem 和 QList<T*> 快照；也可写作 `Conductor<T>::Collection::OneActive`。复制快照不改变集合，指针只借用对象。

| C++ 接口 | 行为 |
| --- | --- |
| addItem(unique_ptr<U>&&) | 接管并按顺序加入，不选择、不初始化或激活；空值失败。 |
| activateItem(unique_ptr<U>&&) | 接管、加入并选择；空 unique_ptr 等同 nullptr。 |
| activateItem(T*) | 选择已有成员，外部对象返回 false；不重复移交所有权。 |
| activateItem(nullptr) | 清空选择并普通停用旧项，集合保留。 |
| closeItem(T*) | 关闭、移除并延迟删除成员；空值、非成员返回 false。 |

新项校验与单项相同，拒绝不移动调用者的 unique_ptr，成功建立父关系并设置 CppOwnership。普通 ViewModelBase 不执行 Screen 生命周期。

切换保留旧项：提交选择、发送 activeItemChanged，普通停用旧 Screen，父活动时激活新 Screen。加入并选择时先提交成员和选择，再依次通知 itemsChanged、activeItemChanged。同项选择不重复通知；父活动时调用其 activate，由 Screen 幂等处理。

关闭当前项按加入顺序优先选前一项；关闭第一项时选后一项，唯一项关闭后留空。先移除旧成员、提交新选择，依次通知 itemsChanged、activeItemChanged，再关闭旧项，父活动时激活新项，最后安排旧项 deleteLater。关闭非当前项不改变选择。通知观察者看到已提交的集合与选择，生命周期转换在通知之后完成。

父 initialize 不初始化成员；激活只激活当前项，普通停用只停用当前项。未初始化父关闭无操作；已初始化父关闭一次清空成员及选择并通知，然后关闭全部成员并延迟删除，不逐项导航。重复关闭空集合不重复通知或回收。析构断开成员连接，由父树兜底，不补生命周期。

意外销毁移除成员；若是当前项则清空并通知，不自动选择其他页。显式 closeItem 才执行相邻自动选择。操作限定主线程同步执行，钩子应正常返回；转换期间回调不得重入修改集合或销毁参与对象，不提供异常恢复协议。

## Shell 示例与元对象适配

4B 历史版本使用单项 Conductor，见 [4B 验收记录](4B验收记录.md)。4C Shell 改为 Collection.OneActive，注入 Home/Detail 两个工厂；home/detail 从集合查找、以 itemsChanged 通知，ViewHost 绑定 activeItem。进入详情保留 Home VM，返回通过 closeItem(detail) 自动回到 Home；Home View 每次重建。完整接口见 [应用装配](IoC与应用装配.md#4c-构造接口与模块边界)。

模板层没有独立元对象。Shell 以 Q_MOC_RUN 分支向 moc 提供 `ConductorCollectionOneActiveViewModelBase`，C++ 编译仍继承泛型集合型；继承关系、元对象父类与 qmllint 均有检查。宏约定见 [Qt moc 文档](https://doc.qt.io/qt-6.8/moc.html)。

WPF CM 的 ViewAware/ViewLocator 可复用已关联且仍可用的 View。本轮只对齐集合型 VM 保留和选择行为，View 保留机制记录在 [后续版本 ToDoList](后续版本ToDoList.md)。

## 验证范围

独立 conductor CTest 直接依赖框架与 Qt Test，不依赖示例模块。覆盖泛型、普通 VM、Screen、通知顺序、延迟回收、所有权拒绝、异线程对象接管拒绝和嵌套生命周期。现有 QML 测试增加不可创建类型及 activeItem → ViewHost 的绑定、替换、清空、意外销毁和释放顺序验证。

4A 核心结果见 [Conductor 核心验收记录](Conductor核心验收记录.md)；4B 的 Shell、服务、宿主重建及实际窗口结果见 [4B 验收记录](4B验收记录.md)。4C 的集合型、Detail 导航与真实窗口结果见 [4C 验收记录](4C验收记录.md)；麒麟仍待验证。
