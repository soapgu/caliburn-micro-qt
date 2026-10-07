# IoC 与应用装配

示例采用 Boost.Ext.DI v1.3.2，在应用装配层递归构造 Shell/Home。Home 在构造 Shell 时创建并接管；初始化钩子用于初始化已有对象。Bootstrapper 驱动根 Shell 生命周期，main 只创建应用并运行 Bootstrapper。当前只有固定 Home 页面，尚未引入 Conductor、业务服务或动态页面工厂。

## 创建与依赖边界

[ViewModelComposition.cpp](../examples/minimal/app/ViewModelComposition.cpp) 提供 `std::unique_ptr<ShellViewModel> buildShell()`：

```cpp
auto injector = boost::di::make_injector();
auto shell = injector.create<std::unique_ptr<ShellViewModel>>();
QQmlEngine::setObjectOwnership(shell.get(), QQmlEngine::CppOwnership);
QQmlEngine::setObjectOwnership(shell->home(), QQmlEngine::CppOwnership);
return shell;
```

当前没有服务绑定，空 injector 已能自动推导构造依赖并递归创建 Home，然后移动其 unique_ptr 给 Shell。返回对象尚未初始化或激活；局部 injector 在函数返回时销毁，VM 继续存活。

业务 VM 构造接口只声明实际依赖：

```cpp
HomeViewModel();
explicit ShellViewModel(std::unique_ptr<HomeViewModel> home);
```

两个业务 VM 不暴露 QObject parent 参数。DI 自动推导 Shell 需要 Home 的 unique_ptr，再通过 Home 的无参构造函数创建 Home；无需声明 ctor_traits 或额外的注入 traits 文件。DI 头文件只由 ViewModelComposition.cpp 包含，业务 VM 可以直接构造供单元测试使用。

Home 与 Shell 创建时均无父对象，Shell 在构造函数中接管 Home。通用框架 ViewModelBase 和 ScreenViewModel 仍保留 parent 参数，供其他使用方选择父所有权；业务 VM 的父关系按当前装配契约建立。

```text
CaliburnExampleApp → CaliburnExampleComposition → CaliburnExampleModule → Caliburn::MicroQt → Qt
                            └─ PRIVATE → Caliburn::BoostDI
```

装配库为普通 C++ 静态库，没有第三个 QML URI。两个 QML 模块仍为 Caliburn.Micro.Qt 1.0 和 CaliburnExample 1.0，保留显式插件导入。DI 单头文件和 Boost Software License 1.0 保存在 third_party/boost-di，配置时校验头文件 SHA-256，构建无需下载依赖。关闭示例和测试后，不添加装配库及 DI 目标。

## 所有权交接

[ShellViewModel](../examples/minimal/CaliburnExample/viewmodels/ShellViewModel.cpp) 构造函数接收 `std::unique_ptr<HomeViewModel>`，要求对象非空、没有既有父对象，且 Shell、Home 和调用者均位于应用主线程。非法参数抛出 invalid_argument，父关系未建立则抛出 runtime_error；Bootstrapper 捕获装配异常，完成清理并非零退出。

合法对象先 setParent(this)，检查 parent 确实为 Shell，再 release 临时 unique_ptr。之后 Bootstrapper 的根 unique_ptr 负责 Shell，QObject 父树负责 Home。Shell 的 QPointer 用于访问和失效保护，Home 意外销毁时显式发送 homeChanged；后续钩子对空 Home 安全跳过。

QML 暴露前，buildShell 明确设置两个 VM 为 CppOwnership。Loader 只拥有 View，不接管 VM。根 Shell 释放时，Home 由 QObject 父树回收一次；注入器既不长期持有对象，也不参与退出回收。

## 生命周期与启动顺序

Shell 显式覆盖以下钩子：

| Shell 钩子 | 子对象操作 |
| --- | --- |
| onInitialize() | home->initialize() |
| onActivate() | home->activate() |
| onDeactivate(close) | home->deactivate(close) |

Screen 基类保留初始化、激活和普通停用幂等；生命周期调用由装配层保证在应用主线程执行。Shell 钩子先完成 Home 的转换，再返回给 Screen 提交 Shell 状态，因此初始化、激活及活动对象关闭的状态通知先 Home 后 Shell。停用后关闭仍调用 Home 关闭钩子；已初始化的 Shell/Home 重复关闭仍执行关闭钩子，状态不变时不重复通知；关闭后重新激活不重置初始化或计数。

[main.cpp](../examples/minimal/app/main.cpp) 创建 QGuiApplication 和 AppBootstrapper，再调用 Run。具体启动流程见 [Bootstrapper](Bootstrapper.md)：

1. AppBootstrapper::Configure 登记 Shell/Home 映射和调用 buildShell 的根工厂，并设置样式。
2. OnStartup 调用 DisplayRootViewFor<ShellViewModel>()，按需调用 buildShell 创建 VM 树并设置 QML 所有权。
3. Bootstrapper 查询根 View 映射，调用 shell->initialize()、shell->activate()。
4. 创建引擎并类型化注入 Shell，加载根窗口；成功后进入事件循环。
5. 正常退出或启动失败时关闭根生命周期，再执行 OnExit。
6. 先销毁引擎与 View，再释放根 unique_ptr 和 Home 父树。

Bootstrapper 不单独创建、激活或关闭 Home。ViewRegistry 继续只定位 View，ViewHost 继续只借用 VM 并创建/卸载 View；窗口失焦不触发生命周期。

下一步 4B 先将 Shell 接入现有单项 Conductor，迁移共享计数服务并引入 Home 工厂；4C 再完善 Collection.OneActive 和 Detail 导航。当前源码仍保持上述装配方式，VM 不接收容器或全局服务定位器。

本机验证结果见 [IoC 装配验收记录](IoC装配验收记录.md)。

## 4B 目标设计（待实现）

本节记录下一步设计，不表示当前接口已修改。第四批按 4A 核心已完成、4B Shell 与服务待实现、4C 集合型与 Detail 后续完善推进，见 [实现计划](迭代实现计划.md#第四批阶段划分)。现有装配和验收说明保留在前文；本次不修改源码或新增通过记录。

### 构造接口与模块边界

目标构造接口如下，仅作为设计：

```cpp
using HomeViewModelFactory = std::function<std::unique_ptr<HomeViewModel>()>;

explicit HomeViewModel(std::shared_ptr<CounterService> counterService);
// ShellViewModel : public Conductor<ScreenViewModel>
explicit ShellViewModel(HomeViewModelFactory homeFactory);

// 保持现有根装配接口。
std::unique_ptr<ShellViewModel> buildShell();
```

CounterService 计划作为示例用户模块中的 QObject，提供 count()、canAdd(int)、add(int)、reset() 和 countChanged()，不向 QML 注册。服务保存唯一计数，初始 0、范围 0～5；拒绝非正增量和超过剩余额度的输入，先比较再相加，非法及同值更新不通知。Home 直接读取服务，保留现有文案、操作和守卫通知；只允许缓存上次守卫结果，不再保存计数副本。

Home 要求共享服务非空。Shell 要求工厂非空，创建的 Home 非空且满足现有 Conductor 接管约束；空依赖或接管失败作为构造错误抛出异常。工厂在重建时出现的空返回值或接管失败同样按错误处理，不静默进入空页面。Bootstrapper 的启动异常处理接口保持不变；不扩展通用生命周期异常恢复协议。

HomeViewModelFactory 的契约归用户模块，具体闭包及 DI 绑定归应用装配层。业务 VM 和框架不包含 DI，不增加容器或服务定位器入口，也不增加第三个 QML 模块。

### 创建与所有权

每次 buildShell() 创建一个独立 `shared_ptr<CounterService>`，由应用装配层显式绑定共享服务和 Home 工厂。工厂只按值捕获该共享服务，每次调用创建新的局部注入器，绑定同一服务并构造 `unique_ptr<HomeViewModel>`；不得捕获局部注入器或 Shell，也不通过全局单例共享计数。

Shell 构造时调用工厂创建初始 Home，交给 activateItem 接管；此时 Shell 非活动，Home 保持未初始化、未激活。Conductor 建立 QObject 父关系、设置 CppOwnership，再释放临时 unique_ptr；buildShell 和 Bootstrapper 继续负责根 Shell 的 CppOwnership，根 unique_ptr 持有 Shell。

服务不设置 QObject 父对象，由工厂与 Home 的 shared_ptr 保持寿命。Shell 关闭后即使旧 Home 已释放，工厂仍持有服务，后续重建可读到原计数；Shell 析构时仍待删除的 Home 也持有服务，不依赖 Shell 成员与 QObject 子对象的析构先后。VM 由 QObject 父树删除，服务由 shared_ptr 管理，两者不同时接管同一对象的删除责任。

### 生命周期与页面重建

| 操作 | 4B 目标行为 |
| --- | --- |
| 构造 Shell | 通过工厂创建并接管初始 Home，不初始化或激活。 |
| initialize() | 仅初始化 Shell，不提前初始化 Home。 |
| activate() | onActivate 在当前项为空时创建 Home，再调用 Conductor 基类实现初始化并激活当前项。 |
| 普通停用再恢复 | 保留同一个 Home 和计数，恢复时不调用工厂。 |
| 未初始化时关闭 | 沿用 Screen/Conductor 契约，无操作，保留当前项。 |
| 已初始化时关闭 | 清空 activeItem 并通知，关闭旧 Home 后 deleteLater，保留服务。 |
| 关闭后重新激活 | 创建新的 Home，使用原服务及计数，不复用已安排删除的旧 VM。 |
| 当前项意外销毁 | 清空界面；下一次 Shell 激活时重建，不在销毁通知中自动导航。 |

Shell 删除原手写子生命周期传播，停用和关闭直接沿用 Conductor 实现；仅保留上述 onActivate 扩展。home 只读属性通过 qobject_cast 投影 activeItem，以 activeItemChanged 通知，不另存 Home 指针。ShellView 宿主计划改绑 viewModel.activeItem，业务 HomeView 继续接收明确类型的 HomeViewModel。

所有操作由调用方保证在应用主线程执行，沿用单项 Conductor 的通知与延迟删除顺序。ViewHost 只负责 View，不改变服务或 VM 生命周期；根工厂、入口和 Bootstrapper 的启动接口不变。

### 4C 衔接与待执行验收

4B 只展示 Home，不实现 Detail、集合型或新导航按钮。4C 再完善 Collection.OneActive 并调整 Shell 的组合方式，在导航期间保留 Home；Detail 按需创建，返回 Home 后关闭、移除并延迟释放，两页沿用共享服务。集合型接口与关闭策略留到下一轮设计，不修改普通单项 Conductor 的释放语义。

4B 实现后需验证多个 Home 共享服务、通知与输入边界、不同 buildShell 实例隔离、局部容器销毁后对象可用、停用保留页面、关闭后重建保留计数、父树与延迟删除的单次回收，以及 QML 的 View 先于旧 VM 销毁。构建、CTest、qmllint、仅框架构建、Cocoa 和实际窗口检查完成后再新增独立验收记录；第四批仍待 4C 完成，麒麟继续待验证。
