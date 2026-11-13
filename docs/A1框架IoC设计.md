# A1 框架 IoC 设计

更新日期：2026-11-13。当前采用非模板固定契约与强类型模板实现层，替代此前运行时类型分派方案。原始阶段验收保留在验收目录，不作为本次模板化验收证据。应用设计见 [A1 重构设计](../../../GitLabs/project_expo/red-conf-system/docs/A1重构设计.md)。

## Provider 分层

```text
IInstanceProvider                         固定窗口服务契约
    ↑
InstanceProvider<Injector>                通用模板实现
    ↑
ApplicationInstanceProvider              应用绑定与装配
```

`IInstanceProvider` 保留虚析构，仅要求实现 `windowManager()`。`InstanceProvider<Injector>` 按值持有 injector，通过其 `create<T>()` 提供 `resolve<T>() -> shared_ptr<T>`、`create<T>() -> unique_ptr<T>`，窗口服务转发到 `resolve<IWindowManager>()`。框架不依赖 Boost.DI；应用负责一次性绑定、具体 injector 类型及其头文件可见性。模板层不维护登记表、类型白名单、实例缓存或依赖图，不验证返回值，不包装构造异常。

## 强类型全局门面

```cpp
#include <CaliburnMicroQt/IoC.h>
// 放在应用命名空间；ApplicationInstanceProvider 需有完整定义。
using IoC = TypedIoC<ApplicationInstanceProvider>;
auto service = IoC::Get<MyService>();
auto model = IoC::Create<MyViewModel>();
```

门面从唯一活动绑定获取基类 shared_ptr，再以 `static_pointer_cast<Provider>` 取得具体 Provider，调用模板方法。应用别名必须与实际安装的具体 Provider 类型一致；这里不做动态类型检查。每次调用期间保留强引用，不创建第二个全局 Provider。任意后端可构造类型都可直接调用，无需额外登记。

不提供名称参数、GetAll、resolveAll、SharedInstance、UniqueInstance、BuildUp 或 OwnedInstance。Get 的共享作用域由后端绑定决定；Create 直接使用默认 unique_ptr，不将共享对象转换为独占对象。编译错误由后端报告，构造异常原样传播；仅绑定错误沿用 InstanceError 诊断。

## Bootstrapper 与生命周期

`setInstanceProvider(shared_ptr<IInstanceProvider>)`、`registerRootFactory<T>()` 和 `displayRootViewFor<T>()` 的对外签名不变。configure 只安装 Provider，配置成功后绑定全局入口，再通过 `windowManager()` 获取同一窗口服务。未配置 Provider 的应用继续使用默认窗口服务。

根模型必须登记显式根工厂；删除通过基类 Provider 自动创建任意根类型的回退。工厂收到框架实际窗口服务，可以捕获具体 Provider 或调用本应用的强类型 IoC。配置阶段不能调用尚未发布的全局门面。

继续检查安装阶段、空 Provider、重复安装、应用线程、绑定冲突和绑定有效期。窗口服务须无 QObject parent、位于应用线程且实现同一对象上的 IWindowManagerLifecycle。请求类型和返回值不增加运行时检查。

清理顺序保持：准备退出 → 根关闭及 onExit → 释放窗口／View／借用 → 释放根及工厂 → 断开连接 → 按身份解绑 → 释放窗口服务及 Provider。失败清理也遵循此顺序；失败的第二个 Bootstrapper 不得解除既有绑定。无额外退出钩子，Conductor 和 View 回收协议保持不变。

## 兼容与验证

旧运行时 Provider API 不保持源码或 ABI 兼容，使用者必须重建并迁移。非模板基类指针只用于固定契约和生命周期；任意类型创建必须经具体 Provider 或 TypedIoC。未配置 Provider、使用显式根工厂的原有示例继续支持。

验证模板后端任意类型创建、共享身份、独占释放、原始异常、全局与直接调用一致性、绑定冲突／失效／跨线程，以及根工厂、配置失败和窗口关闭回收。独占接口转换沿用 C++ 智能指针及后端规则，不再提供框架类型擦除包装或额外映射校验。

本次本机验证记录见 [Provider 模板化验收](验收/Provider模板化验收.md)。
