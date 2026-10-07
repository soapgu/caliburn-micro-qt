# IoC 与应用装配

4B 已实现 Shell 单项 Conductor、共享计数服务及关闭后重建，见 [4B 验收记录](4B验收记录.md)。第三批原装配的历史结果见 [IoC 装配验收记录](IoC装配验收记录.md)，其 Home 无参构造与 Shell 直接接收页面的接口已由下述接口替代。

## 4B 构造接口与模块边界

```cpp
explicit HomeViewModel(std::shared_ptr<CounterService> counterService);
using HomeViewModelFactory = std::function<std::unique_ptr<HomeViewModel>()>;
explicit ShellViewModel(HomeViewModelFactory homeFactory);
std::unique_ptr<ShellViewModel> buildShell();
```

CounterService 位于示例用户模块 services/，继承 QObject，不注册为 QML 类型，不设置 QObject 父对象。它提供 int count() const、bool canAdd(int) const、void add(int)、void reset() 和 countChanged()。计数唯一保存在服务中，初始 0、范围 0～5，先比较剩余额度再相加；非正、超范围和无变化操作不修改、不通知。

Home 构造必须提供非空服务，没有无参构造或隐式服务，空服务抛出 std::invalid_argument。它保留原 QML 属性、文案、方法和守卫，直接读取服务、委托操作，只缓存上次守卫结果。构造按服务当前状态初始化缓存，不发送初始通知；服务变化时先更新缓存，再发送 countChanged 及实际改变的守卫通知，使通知观察者读到一致状态。

Shell 运行时继承 Conductor<ScreenViewModel>，仅保存工厂。构造调用工厂，通过 activateItem 接管初始 Home，页面仍未初始化、未激活。空工厂、空返回值和接管失败抛出 std::invalid_argument，工厂自身异常原样传播。home 只读属性用 qobject_cast 投影 activeItem，以继承的 activeItemChanged 通知；不保存第二份页面指针。业务 VM 与框架不包含 DI 头文件或容器接口。

## 创建与所有权

[ViewModelComposition.cpp](../examples/minimal/app/ViewModelComposition.cpp) 每次 buildShell() 创建独立 shared_ptr<CounterService>。Home 工厂仅按值捕获服务，每次调用创建局部 Boost.Ext.DI 注入器，绑定同一服务并创建 unique_ptr<HomeViewModel>；根注入器将该工厂绑定给 Shell，返回根 unique_ptr。闭包不捕获注入器或 Shell，不存在全局计数单例。

DI 是装配库的私有依赖。局部注入器离开作用域后，工厂和 Home 的 shared_ptr 继续保持服务寿命。服务无 QObject 父对象；页面通过 Conductor 接管建立 QObject 父关系和 CppOwnership，并释放临时 unique_ptr。根 Shell 由返回的 unique_ptr 管理，Bootstrapper 继续只驱动根生命周期。创建、服务更新及生命周期操作由应用保证在主线程执行。

关闭后的旧页面可能仍等待 DeferredDelete，此时工厂和旧、新页面可以共同持有服务；旧页面析构不会清空新的选择。Shell 提前析构时，父树回收当前页和仍存活的旧页，服务在最后一个 shared_ptr 释放时销毁一次。

## 生命周期与页面重建

| 操作 | 当前行为 |
| --- | --- |
| Shell 构造 | 创建并选择 Home，不初始化或激活。 |
| Shell.initialize() | 只初始化 Shell。 |
| 首次激活 | 初始化并激活 Home，然后提交 Shell 活动状态；通知依次为 Shell 初始化、Home 初始化、Home 激活、Shell 激活。 |
| 普通停用再激活 | 保留 Home 身份及计数，不再调用工厂。 |
| 未初始化 Shell 关闭 | 沿用 Screen 无操作规则，保留页面。 |
| 已初始化 Shell 关闭 | 清空 activeItem，宿主卸载 View，关闭旧项并安排延迟删除。 |
| 关闭后激活 | 空项时通过工厂创建新 Home，沿用原服务及计数；不要求旧项已处理删除事件。 |
| 当前项意外销毁 | 清空界面，不在销毁通知中自动导航；Shell 停用后再次激活才重建。 |
| 重建工厂失败 | 异常传播，选择仍为空，Shell 保持非活动，已有服务状态保留。 |

Shell 只重写 onActivate()：当前项为空时创建 Home，再调用 Conductor 基类实现。初始化、停用和关闭不再手写子生命周期传播。ViewHost 绑定 activeItem，HomeView 仍声明 required property HomeViewModel viewModel。启动、类型化注入和根清理职责见 [Bootstrapper](Bootstrapper.md)，宿主所有权见 [ViewHost](ViewHost.md)。

## 验收与 4C 衔接

服务边界、共享同步、装配隔离、工厂失败、生命周期、页面及服务单次回收、宿主重建、View 先于 VM 销毁均已覆盖。构建、CTest、qmllint、仅框架构建、Cocoa 与实际窗口回归结果见 [4B 验收记录](4B验收记录.md)。

4B 只展示 Home。4C 再完善 Collection.OneActive 并调整 Shell 组合方式：Home 在导航期间常驻，Detail 按需创建，返回后关闭、移除并延迟释放，两页沿用共享服务。集合型接口与关闭策略待设计，不改变单项 Conductor 核心契约，不创建占位类型。第四批仍为进行中，麒麟待验证，见 [阶段计划](迭代实现计划.md#第四批阶段划分)。
