# Bootstrapper 与应用启动

框架提供纯 C++ 的 `BootstrapperBase`，应用派生 `AppBootstrapper`。它不向 QML 注册类型，不依赖 Boost.Ext.DI，不创建 QGuiApplication。应用先创建 QGuiApplication，再创建 Bootstrapper，在主线程调用一次 `run()`；应用对象必须比 Bootstrapper 活得更久。

## 命名约定与迁移

`BootstrapperBase` 的所有普通成员方法采用 Qt 风格 camelCase，与 Caliburn.Micro 保持概念对应；不要求与 C# 原版的方法拼写一致。类名继续使用 PascalCase。

本次改名属于破坏性接口变更，旧名称已移除，不提供兼容别名。使用方需同步修改调用点及派生类的 `override` 声明、定义，然后重新编译框架和应用；方法签名、访问级别和生命周期行为保持不变。

| 旧名称 | 当前名称 |
| --- | --- |
| `Run()` | `run()` |
| `Configure()` | `configure()` |
| `OnStartup()` | `onStartup()` |
| `OnExit()` | `onExit()` |
| `RegisterRootFactory<T>()` | `registerRootFactory<T>()` |
| `DisplayRootViewFor<T>()` | `displayRootViewFor<T>()` |

私有辅助方法也统一改为 camelCase。历史验收、计划和评估文档保留当时的名称，当前接口以本页及头文件为准。

## 应用入口与扩展点

```cpp
int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    AppBootstrapper bootstrapper(app);
    return bootstrapper.run();
}
```

静态模块的 Q_IMPORT_QML_PLUGIN 仍放在入口，链接要求保持不变。

| 接口 | 契约 |
| --- | --- |
| `int run()` | 编排配置、启动、事件循环和清理。同一实例只能调用一次；重复调用返回 1，不重复执行退出钩子。 |
| `bool configure()` | 应用设置样式、登记业务 View 映射和根工厂；返回 false 则停止启动，不自动冻结注册表。成功后由 Bootstrapper 显式冻结，再进入 onStartup。 |
| `void onStartup()` | 默认空实现的虚方法；在 configure 成功并冻结映射后调用。应用显示根窗口，显示失败、重复显示或未显示均判定启动失败。 |
| `void onExit()` | 默认空实现的虚方法。正常退出在 aboutToQuit 中、根关闭之后调用；失败清理补调。最多调用一次，成功展示后的根 VM 与 View 在钩子中仍存活；展示失败的部分窗口资源已经回收，须兼容部分初始化。 |
| `bool registerRootFactory<T>(std::function<std::unique_ptr<T>(std::shared_ptr<IWindowManager>)> factory)` | 只允许在 configure 中登记，每个类型只能登记一次；拒绝空工厂，登记时不创建对象。工厂统一接收框架窗口服务并返回 unique_ptr<T>。 |
| `bool displayRootViewFor<T>(RootViewOptions options = {})` | 只允许在 onStartup 中调用；每个 Bootstrapper 只允许一次显示尝试，T 必须继承 ScreenViewModel。RootViewOptions 当前为空，预留根窗口设置扩展。 |

示例 AppBootstrapper::configure 设置 Basic 样式，配置 `qrc:/qt/qml/CaliburnExample/` 搜索根，使 Shell/Home/Detail 通过约定定位，并用 `[](std::shared_ptr<IWindowManager> windows) { return buildShell(std::move(windows)); }` 提供 Shell 工厂。onStartup 只调用 `displayRootViewFor<ShellViewModel>()`。当前装配通过 Home/Detail 工厂和共享服务构造 Shell，显示时可省略选项，也可显式调用 `displayRootViewFor<ShellViewModel>(RootViewOptions{})`，行为相同。

## 工厂与所有权

Bootstrapper 保存按 VM 元对象地址索引的根工厂；在 configure 前创建并持有独立 WindowManager，调用根工厂时提供同一服务。不提供应用级服务定位或窗口服务实现替换入口；业务服务与子 VM 的依赖仍由应用装配层解决。

registerRootFactory 直接把工厂保存为统一返回 unique_ptr<ScreenViewModel> 的 std::function，使用标准类型转换，不增加包装函数。displayRootViewFor 先查表调用工厂，校验并把 VM 保存到 m_root；然后取出具体 T* 构造 QVariant，委托 IWindowManager::showWindow。displayRootView 只记录显示结果，不创建引擎、窗口或 WindowConductor。

m_root 负责根 VM 最终删除及失败/强制退出时的生命周期兜底；正常窗口关闭后的 VM 停用由 WindowManager 执行。窗口服务借用根 VM，并持有自己的引擎、窗口和私有桥接，不强持有根 VM，避免与业务持有的共享窗口服务形成所有权环。QVariant 保留具体指针类型供 QML required 属性注入，不接管对象。RootViewOptions 只在显示入口接收，当前不保存、不影响行为。

根对象必须无 QObject 父对象，并位于应用主线程。Bootstrapper 在暴露根 VM 前设置 CppOwnership；子对象所有权由 Conductor 接管流程设置，Bootstrapper 不递归猜测对象图。

## 自动关联与独立窗口展示

WindowManager::showWindow 创建根 QQuickWindow 后自行登记所属窗口，并显式显示窗口，不修改业务内容树；服务按需创建独立的 ApplicationModal 窗口。ShellView 不需要声明弹窗宿主，ShellViewModel 不需要暴露窗口服务。ViewLocator 自动提供框架搜索根，内置确认视图通过约定定位；应用可配置业务搜索根并部署同名视图，或显式登记覆盖。

不经过 Bootstrapper 时，同样通过 showWindow 显示借用 VM 的普通窗口，再展示弹窗。attachToWindow / detachFromWindow 是 WindowManager 私有的内部关联机制，不再支持外部 QML 窗口手动接入。Bootstrapper 的根工厂统一接收框架窗口服务。

根 View 创建和绑定先于根模型激活。激活前窗口服务建立所属窗口、引擎、关闭桥接和窗口弱关联，因此根激活钩子可以直接 tryClose，或使用注入的 IWindowManager 打开确认弹窗。已有所属窗口或重复 Screen 关联时在创建和激活前拒绝；加载失败不激活，激活异常释放 View 并执行一次关闭收尾。

## 启动与退出顺序

1. run 在应用主线程创建 WindowManager，再执行 configure，不在构造函数里调用虚方法。
2. 配置成功后冻结 ViewLocator，进入 onStartup，调用根工厂。
3. displayRootView 委托 showWindow 校验模型、所属窗口和本地/qrc View 地址。
4. 服务创建引擎并注入模型和初始 visible=false，加载并校验窗口，建立所属窗口、弱关联和关闭桥接，调用 create() 创建隐藏的原生资源。
5. 调用可选 IActivate；Screen 自动初始化。随后确认对象仍有效且未完成关闭，显式显示窗口。
6. onStartup 成功返回后进入事件循环；窗口关闭经可选守卫，由服务执行一次模型关闭生命周期。
7. aboutToQuit 准备窗口退出并结束在途弹窗，兜底关闭根，再执行一次 onExit，保留普通 View 和根模型。
8. 事件循环返回后补齐尚未执行的退出通知，释放桥接、View、引擎，最后删除根模型和工厂。外部共享服务引用不延长根 View 寿命。

根对象关闭最多尝试一次，包括业务直接执行根 deactivate(true) 的情况；退出钩子调用一次。基类析构只兜底清理，不调用派生类 onExit。run 的清理路径捕获关闭钩子和 onExit 的异常，继续释放剩余资源；异常不会从 aboutToQuit 回调逸出。

配置失败、缺少工厂、空对象、工厂或启动异常、缺少映射、QML 加载失败及根对象不是窗口都打印诊断并返回 1。正常退出保留 Qt 事件循环退出码；如果原退出码为 0 但清理失败，返回 1。

第一版仅支持一个根窗口、本地或 qrc QML 和同步 Screen 生命周期。远程 QML、异步生命周期、多窗口管理和运行时模块加载不包含在当前接口中。

## 4C 集合导航接入与退出

displayRootViewFor 接收当前为空的 RootViewOptions，run 接口不变；buildShell 提供接收框架窗口服务的重载。AppBootstrapper 配置 Shell/Home/Detail 映射，应用装配层提供共享服务及两个页面工厂。构造只选择 Home，激活由集合型 Conductor 传播。

从 Home 或 Detail 退出都只关闭根 Shell，集合型一次清空所有页面和选择，关闭并延迟回收成员。onExit 时根仍存活，子页可能已处理 DeferredDelete；测试分别验证当前子 View 先于子 VM、根 View 先于根 VM 释放，不要求旧页面活到根析构。

本轮实际示例启动测试从 Home 进入 Detail 后关闭窗口，自动及实际窗口退出均通过，见 [4C 验收记录](验收/4C验收记录.md)。Bootstrapper 不直接管理导航或 View 缓存，历史 [4B 验收记录](验收/4B验收记录.md) 保留原样。

## 验证

窗口服务、自动宿主和默认视图映射的当前验证见 [弹窗基础设施验收记录](验收/弹窗基础设施验收记录.md)。

showWindow 职责迁移及新增所有权/失败清理回归见 [根窗口职责迁移验收记录](验收/根窗口职责迁移验收记录.md)；早期验证过程见 [可行性评估](评估/根窗口职责迁移可行性评估.md)。本次保留当前激活时序，CM 的创建/绑定/激活顺序及普通 Item 窗口包装留待独立变更。

CaliburnBootstrapperTests 验证具体类型注入、Home 装载、根生命周期、View 先于 VM 释放、重复运行保护、失败清理和异常退出，并覆盖 configure 中查询后继续登记、进入 onStartup 前已经冻结以及 configure 失败不自动冻结。各场景由 CTest 在独立进程运行：注册表的映射和冻结状态均为进程级，不同场景需要为同一类型使用不同映射或保留空表；显式冻结不消除这项隔离需求。实际 AppBootstrapper 另有窗口关闭退出的集成场景。

以下保留 2026-10-06 原始 Bootstrapper 验收结果；4B 历史结果见 [4B 验收记录](验收/4B验收记录.md)，当前结果见 [4C 验收记录](验收/4C验收记录.md)。原始环境为 macOS arm64 / Qt 6.8.3：

| 检查 | 结果 |
| --- | --- |
| macos-local 全量构建 | 通过。 |
| CTest | 原有 core、composition、qml 与 16 个 Bootstrapper 场景全部通过，共 19 个测试项。 |
| all_qmllint | 框架与示例均通过。 |
| 仅框架构建及 qmllint | 关闭示例和测试后通过，不加入 Boost DI 或应用装配目标。 |

新增测试使用 offscreen/software 与 Basic 样式；当时未进行人工窗口操作或麒麟验证；4B 已另行完成本机实际窗口回归，麒麟仍待验证。构建与测试在沙箱外执行，避免沙箱内 Qt 工具无法识别 NEON 指令的问题。故意非法的既有 QML 夹具与静态插件重复链接继续产生预期诊断，不影响验收结果。

## Parent 协议与根生命周期

根 Shell 的逻辑 parentViewModel 为空；Bootstrapper 继续持有根 unique_ptr，不实现 IConductor，也不作为逻辑 Parent。退出先关闭根生命周期，再释放根 View/引擎，最后释放根 VM。已初始化单项 Conductor 关闭时仅处理当前项；普通停用后的旧对象不补发关闭生命周期，集合型清理全部成员；QObject 父树兜底回收未处理的延迟删除项。

Screen.tryClose 优先委托逻辑 Parent IConductor，否则直接关闭弱关联窗口；非 Conductor 的逻辑 Parent 也走窗口回退。桥接先拒绝原关闭事件，在调用栈退出后询问根 canClose，获准后再次调用窗口 close；其他 QML 处理仍可拒绝。根直接完成 deactivate(true) 时反向关窗，不重复守卫。普通停用不关窗；无有效关联时只诊断，不缓存请求。

5A/5B 的能力及历史验收继续保留。当前增量补齐根窗口双向桥接及 CM 风格 void 启动、退出钩子，见 [根窗口验收记录](验收/根窗口关闭守卫与生命周期验收记录.md)。旧 bool onStartup 重写需迁移为 void；onExit 从事件循环返回后前移到 aboutToQuit。配置和启动失败仍补调退出通知。直接 exit 或强制清理不询问守卫，多窗口和系统强制终止交互不在范围内。

独立模态窗口改造后，aboutToQuit 和最终 shutdown 都在根生命周期之前解除窗口服务关联，强制完成弹窗 Future，并保证弹窗 View/窗口先于引擎释放。onExit 时根 VM 和根 View 仍存活。见 [独立模态窗口验收](验收/独立模态窗口验收记录.md)。

第七批将配置冻结与根窗口定位统一迁入 [ViewLocator](ViewLocator.md)，示例通过模块资源根定位 Shell/Home/Detail。后续 QObject 接口化已放宽根工厂类型，本轮进一步采用先创建绑定、再激活显示及直接关闭弱关联窗口的顺序，所有权和退出钩子寿命保持不变。
