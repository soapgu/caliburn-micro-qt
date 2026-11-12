# Bootstrapper 提供器接入验收记录

日期：2026-11-12。源码基线：`4e33d20`，对应 [IoC 改造方案](../IoC改造方案.md)第 4 步。本记录不等同于下游 F2 兼容验收。

## 实现与所有权

新增受保护的 setInstanceProvider(shared_ptr<IInstanceProvider>)，只允许 configure 中登记一次，拒绝空值。保存期间不解析；configure 成功后调用未命名 resolve<IWindowManager>()，验证无 parent、应用线程及 IWindowManagerLifecycle 能力，订阅成功后进入 ViewLocator 冻结和 onStartup。configure 失败不解析提供器。

configure 成功后一次性选择服务：无提供器则创建默认 WindowManager，有提供器则只解析应用服务，不再提前创建及替换临时默认服务。configure 返回 false 或抛异常时不创建框架默认服务。实际服务通过共享指针持有，生命周期能力从同一对象 qobject_cast 取得并借用，不新增控制块或独立清理对象。已通过线程检查且具有清理能力的候选纳入失败清理；线程错误或缺少能力时不调用候选清理方法，不回退默认路径继续启动。

旧根工厂保留原调用签名，并优先于提供器。仅无该类型旧工厂时才使用 create<T>()，创建失败不尝试其他入口。根保存为 OwnedInstance<QObject>，旧工厂使用正常 QObject 删除方式，提供器根结果保留原删除器；先构造删除器再释放源指针。根须非空、无 parent、位于应用线程，登记与请求的类型必须严格对应，QML 仍获得具体 T* 的 QVariant。

准备退出和释放窗口统一通过生命周期接口调用；清理失败订阅替代具体 WindowManager 信号连接。aboutToQuit 和最终 shutdown 捕获准备、释放异常，继续根关闭、onExit 及后续清理。清理失败将退出码 0 改为 1，非零退出码保留。自定义服务必须在清理失败时仍解除模型借用。

退出顺序：准备窗口 → 根关闭／onExit → 释放窗口及 View → 释放根和工厂 → 断开服务连接并清空借用 → 释放服务与提供器的框架引用。外部共享引用不会延长窗口和根 View 寿命。基类析构不调用派生 onExit。

未新增具体容器、全局 IoC 或自动 BuildUp；WindowManager 清理实现、示例和下游应用均未修改。

## 新增测试

新增独立程序 CaliburnBootstrapperProviderTests、类型化 QML 夹具 ProviderRoot.qml 和 30 个 bootstrapper_provider_* CTest 场景，按进程隔离映射／冻结状态。

| 场景 | 断言 |
| --- | --- |
| success、normalClose | 提供器独占根创建及正常退出／窗口关闭；类型化 QVariant、退出钩子存活性、View 先于根释放 |
| dialogIdentity | 实际根窗口和弹窗使用解析得到的同一窗口服务 |
| legacy、legacyNull、legacyThrows | 旧工厂优先、接收实际窗口服务；空结果或异常不调用提供器根创建 |
| nullSet、repeatSet、configureFailure、configureThrows | 设置限制及配置失败／抛异常不解析、onStartup 不执行、onExit 一次、提供器引用释放、退出码 1；所有场景另断言 run 前后设置失败、重复 run 不重复执行 onExit |
| serviceMissing、serviceThrows | 窗口解析缺失／异常，onStartup 不执行，不回退 |
| missingLifecycle、serviceParent、serviceThread | 缺少能力、parent 与线程校验；错误线程不调用服务清理 |
| subscriptionFailure、subscriptionThrows | 订阅无效或抛异常，候选服务仍被清理，onStartup 不执行 |
| rootMissing、rootThrows、rootNull、rootWrongContract | 根缺失、异常、空结果及错误契约报告失败 |
| rootParent、rootThread | 根所有权／线程校验拒绝，实例回收一次 |
| showFailure | 创建后 QML 映射加载失败，根回收一次，窗口清空 |
| prepareThrows、releaseThrows | 测试服务履行资源清理后注入异常；异常不越过 Qt 边界，窗口和根仍释放，退出码 1 |
| cleanupSignal、exitCodeAndCleanup | 接口通知导致退出失败；已有退出码 7 不被清理失败覆盖 |
| retained | 外部保留服务但窗口和根已释放，run 后发出失败信号不再执行旧订阅 |
| sequential | 同一进程串行两个独立提供器上下文，复用已冻结的同一 View 映射，旧上下文无后续解析 |

窗口解析均断言 configure.end 在 resolve 前、resolve 在 startup 前。已创建根在成功和显示／校验失败路径检查析构次数，正常路径检查 root.destroy 先于 provider.destroy；提供器框架引用在 run 返回前释放。

## 初始化时序简化

当前实现将默认服务创建后移至 configure 成功之后。核对结果：m_windowManager 是私有成员，registerRootFactory 只保存工厂，根工厂在 onStartup 中才执行，配置阶段没有窗口服务访问依赖。selectWindowService 仅执行首次接管、验证与订阅，删除旧服务释放、替换及相同对象判断分支。最终 shutdown 的解绑和两阶段清理顺序保留。

新增 configureThrows 场景：登记提供器后在 configure 抛异常，断言零解析、零根创建、onStartup 不执行、onExit 一次、提供器框架引用释放及 run 返回 1。未添加构造观测钩子；configure 前无窗口服务创建及两个选择分支互斥由源码核对确认。

历史《IoC改造前旧行为基线》保持原文，记录当时默认服务在 configure 前创建的事实；当前调用时序以本记录和 Bootstrapper 文档为准。工作区已有的实际类型校验及 actualType 场景移除保持原状，因此当前 29 个提供器场景加 configureThrows 为 30 个，总数为 141 项，而非原计划预计的 142 项。

## 执行过程与结果

环境：macOS arm64、Qt 6.8.3、CMake 3.22.1、Ninja 1.12.1，复用 Debug 构建；GUI 测试使用 offscreen/software 和 Basic 样式。

顺序场景通过测试状态显式标记映射复用，没有增加生产注册表查询或重置接口。首次定向运行发现测试夹具计数遗漏 releaseWindows 内部再次准备，以及准备异常注入破坏测试服务自身的释放保证；仅修正夹具计数与异常注入位置，生产 WindowManager 清理逻辑未改。修正后定向及完整测试通过。后续补强旧工厂服务身份与最终解绑断言后重新执行全部验证。

```sh
cmake --build --preset debug
ctest --test-dir build/debug -R '^bootstrapper_provider_' --output-on-failure
ctest --preset debug
cmake --build build/debug --target all_qmllint
cmake -S . -B build/ioc-contract-framework-only -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH=/Users/guhui/Qt/6.8.3/macos \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCALIBURN_BUILD_EXAMPLE=OFF -DCALIBURN_BUILD_TESTS=OFF
cmake --build build/ioc-contract-framework-only
git diff --check
```

| 检查 | 结果 |
| --- | --- |
| Debug 构建 | 通过 |
| 新增定向 CTest | 30/30 通过，2.03 秒，包含 configureThrows |
| 完整 CTest | 141/141 通过，32.86 秒 |
| all_qmllint | 通过，框架和三个示例模块均通过 |
| 纯框架配置与构建 | 通过 |
| 差异检查 | git diff --check 通过 |

本轮初始化简化验证日志在忽略的 build/debug/bootstrapper-initialize/ 目录，前一版本记录在 build/debug/bootstrapper-provider/ 目录。日志文件包括：build.log、targeted.log、ctest.log、qmllint.log、framework-configure.log、framework-build.log。逐项详细输出可查 build/debug/Testing/Temporary/LastTest.log，后续运行会覆盖。

## 边界

本轮交付框架可选接入及其测试，不交付应用容器实现，不迁移业务。接口返回对象的所有权及创建线程由应用适配器保证；错误线程或没有生命周期能力的服务不由框架跨线程强行清理。自定义服务必须遵守两阶段清理、失败仍解除借用及回调不抛异常的契约。

步骤 5 的进一步失败专项验证、步骤 6 的框架交付回归及步骤 7 的下游 F2 仍按方案独立推进。未执行真实桌面、现场四屏、麒麟及红厅兼容重建，不承诺预编译二进制 ABI 兼容。本轮不提交推送。
