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

Screen 基类的幂等与重入检查继续有效。Shell 钩子先完成 Home 的转换，再返回给 Screen 提交 Shell 状态，因此初始化、激活及活动对象关闭的状态通知先 Home 后 Shell。停用后关闭仍调用 Home 关闭钩子；重复关闭无操作；关闭后重新激活不重置初始化或计数。

[main.cpp](../examples/minimal/app/main.cpp) 创建 QGuiApplication 和 AppBootstrapper，再调用 Run。具体启动流程见 [Bootstrapper](Bootstrapper.md)：

1. AppBootstrapper::Configure 登记 Shell/Home 映射和调用 buildShell 的根工厂，并设置样式。
2. OnStartup 调用 DisplayRootViewFor<ShellViewModel>()，按需调用 buildShell 创建 VM 树并设置 QML 所有权。
3. Bootstrapper 查询根 View 映射，调用 shell->initialize()、shell->activate()。
4. 创建引擎并类型化注入 Shell，加载根窗口；成功后进入事件循环。
5. 正常退出或启动失败时关闭根生命周期，再执行 OnExit。
6. 先销毁引擎与 View，再释放根 unique_ptr 和 Home 父树。

Bootstrapper 不单独创建、激活或关闭 Home。ViewRegistry 继续只定位 View，ViewHost 继续只借用 VM 并创建/卸载 View；窗口失焦不触发生命周期。

后续引入 Conductor 时再将通用子项生命周期管理交给它；动态详情页通过应用提供的类型化工厂按需创建。当前 VM 不接收容器或全局服务定位器。

本机验证结果见 [IoC 装配验收记录](IoC装配验收记录.md)。
