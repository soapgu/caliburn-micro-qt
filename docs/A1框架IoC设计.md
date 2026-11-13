# A1 框架 IoC 设计

设计日期：2026-11-13。状态：框架配套已完成本机验收；应用生产接入仍待应用阶段。OwnedInstance 已删除，独占结果统一默认 unique_ptr。本文与应用 [A1 重构设计](../../../GitLabs/project_expo/red-conf-system/docs/A1重构设计.md) 配套；应用侧文档负责业务范围，本文负责框架契约。F1／F2 已通过的历史测试不等于新 A1 已验收。

## 1. 目标与边界

框架提供全局 `IoC::Get/Create/GetAll`，只依赖 `IInstanceProvider`。应用 `ApplicationInstanceProvider` 完全内置 Boost.DI；框架不引入 Boost.DI、红厅类型、业务绑定或反射构造功能。

```text
IoC::Get<T>() / IoC::Create<T>() / IoC::GetAll<T>()（框架）
                  ↓
          IInstanceProvider（框架）
                  ▲ 实现
     ApplicationInstanceProvider（应用）
                  ↓
         私有 Boost.DI injector
```

Bootstrapper、IoC 和应用动态创建调用方必须使用同一个 Provider。IoC 不保存第二套对象表或单例缓存，也不改变构造注入及 ViewHost／窗口服务的视图装配职责。

## 2. 门面接口

已新增公开头文件 `CaliburnMicroQt/IoC.h`，并加入框架构建清单。以下为当前框架 API。

| 接口 | 返回 | 转发到当前 Provider |
| --- | --- | --- |
| `IoC::Get<T>(const QString &name = {})` | `std::shared_ptr<T>` | `resolve<T>(name)` |
| `IoC::Create<T>(const QString &name = {})` | `std::unique_ptr<T>` | `create<T>(name)` |
| `IoC::GetAll<T>()` | `std::vector<std::shared_ptr<T>>` | `resolveAll<T>()` |

`Get/Create/GetAll` 采用本次确认的 CM 风格名称，现有 Provider 与 Bootstrapper 方法仍保持 camelCase；文档与头文件对这一例外使用一致拼写。Provider 的 create 返回类型为默认 unique_ptr，当前只保留 resolve、create、resolveAll；删除 tryResolve、buildUp 与 BorrowedInstance。A1 不新增 TryGet 或 BuildUp 门面，不增加 exposeShared/exposeUnique、登记 API、容器 getter 或框架绑定描述语法。

门面保留名称、类型、共享控制块、默认删除所有权及已有错误传播规则。未初始化、已解绑或弱引用失效属于上下文不可用错误；建议以现有 `InstanceErrorCode::CreationFailed` 配合请求操作／类型和明确原因报告，区别于已绑定 Provider 返回的 NotRegistered。Get 不自行决定单例策略，Create 不把共享／借用对象转换成独占对象。

### 2.1 应用一次性装配边界

应用在启动组合根中一次性调用 di::make_injector，完成接口映射、集合绑定与作用域配置，再将完整 injector 交给应用 Provider 持有。框架不引入 Boost.DI，也不通过泛型虚方法获取容器。运行时 type_index 请求到编译期 injector.create<T>() 的转发由应用适配器集中实现，不另建业务缓存、依赖图或递归构造规则。

resolveAll<T>() 返回适配器显式提供的集合，可转发到 injector.create<std::vector<std::shared_ptr<T>>>()；原样保留顺序和重复项，不从共享单实例登记扫描、合并结果。已实现集合能力可返回空，不支持则明确报错。

这是公开 API 收缩，原 tryResolve/buildUp 调用方需要调整。红厅应用现有 exposeShared/exposeUnique 适配器及其测试仍待后续修改，本轮不执行其兼容验收。见 [接口收缩验收](验收/实例提供器接口收缩验收记录.md)。

## 3. 绑定所有权与生命周期

绑定由 Bootstrapper 内部控制，不增加任意业务代码可覆盖全局 Provider 的公开 setter。全局状态保存弱引用及绑定身份；Bootstrapper 强持有 Provider。一次只允许一个活动绑定，不覆盖、不堆叠，也不自动恢复被覆盖的旧状态。

### 3.1 启动时序

1. `configure()` 中的 `setInstanceProvider` 只登记一次，拒绝空值，不解析且不立即发布全局入口。
2. 配置失败或抛异常时不发布；配置成功后、窗口服务解析前取得活动绑定。
3. 获取绑定冲突则明确终止本次启动，不解析新 Provider，不清除既有活动绑定。
4. 通过已登记的同一个 Provider 解析 `IWindowManager`，验证线程、无 parent 及同一对象上的 `IWindowManagerLifecycle`；进入 `onStartup()`。
5. 原根工厂优先规则不变。旧根工厂执行时全局 IoC 已生效，但 configure 期间不能依赖它；配置期间确需解析可显式调用尚在配置的 Provider。

未配置 Provider 的旧应用不自动获得另一套全局容器，继续默认窗口服务和旧根工厂。注册新 Provider 后服务解析失败不回退默认对象；根选定路径失败不尝试其他路径。门面和生命周期控制在应用线程执行，不新增跨线程容器保证。

### 3.2 清理时序

正常退出及启动失败沿用现有顺序：准备窗口退出 → 根关闭与 onExit → 释放 View／引擎／借用 → 释放框架根及根工厂 → 断开窗口连接 → 按绑定身份解绑 IoC → 释放窗口服务与 Provider 引用。

不新增 onWindowsReleased 或其他退出钩子，不要求应用辅助对象提前显式释放；它们继续沿用原有成员及共享所有权方式。全局绑定覆盖框架清理过程，不延长到应用派生对象成员最终析构。清理单步抛异常也继续解绑；析构兜底不调用派生 onExit。没有取得绑定的实例不能解绑其他实例。解绑后外部仍持有的 shared_ptr 可以继续存活，但不能再次经全局入口解析旧上下文。

## 4. 彻底移除 OwnedInstance，保留默认 unique_ptr 方式

### 4.1 当前需求核查

当前应用和框架示例的页面、根 VM、弹窗均使用默认 unique_ptr，生产 IoC 对象未发现特殊释放需求。应用 OpenSSL 上下文和 libxls 工作簿／工作表虽然有特殊释放函数，但都局限于适配器内部，不经过 IoC。详细证据见 [应用 A1 核查表](../../../GitLabs/project_expo/red-conf-system/docs/A1重构设计.md#51-当前代码核查结论)。

框架 [tst_instanceprovider.cpp](../tests/cpp/tst_instanceprovider.cpp) 的 Contract 故意没有虚析构，独占测试通过它返回派生 Implementation，属于原额外能力的验证，不是生产用例。该独占保证撤销；共享接口测试仍可保留，因为 shared_ptr 控制块保存真实删除方式。UniqueInstance::from 继续只接受默认 unique_ptr，不提供特殊删除器注册。当前内部改为类型化 RAII holder，校验后移出默认 unique_ptr，不捕获或传递实例删除器。

### 4.2 新独占契约

- 删除公开 OwnedInstance 别名，不以兼容别名继续传播复杂删除器类型。
- `IInstanceProvider::create<T>()` 和 `IoC::Create<T>()` 返回默认 `std::unique_ptr<T>`。
- 强类型独占映射仅支持：Implementation 与 Contract 相同；或者 Contract 具有可访问虚析构且 Implementation 指针可合法转换为 Contract 指针。后者可通过编译期约束检查，禁止通过非虚析构基类默认删除派生对象。
- 共享解析不增加上述虚析构限制；不把当前共享接口用例误判为不能使用 IoC。
- 不支持数组和特殊分配／回收策略，未来按实际需求另设方法或路径。局部资源的自定义 RAII 保持在适配器内部。

运行时请求仍需内部 RAII 结果包装：构造后若类型／空值校验失败，包装必须回收对象；成功时才把已验证的契约指针交给默认 unique_ptr。内部可以保留默认删除所必需的类型擦除辅助，但其 from 入口必须实施上述安全映射约束，不能继续接受“非虚析构基类 + 派生对象”再偷偷丢掉实际类型删除器。不要将包装简化为失败路径无人释放的裸 void*。

### 4.3 启动与回收链

Bootstrapper 根持有、根创建辅助和旧根工厂结果统一使用默认 `unique_ptr<QObject>`；移除原 get_deleter、删除器闭包和 OwnedInstance 转接。根模型为 QObject 派生对象，继续满足无 parent、应用线程和具体 T* QML 注入要求。

Conductor、内部退役回收设施和 View 借用协议保持现有实现及接口，不新增 OwnedInstance 消费、删除器存储或父析构交接机制。应用输出工厂继续返回默认 `unique_ptr<IOutputContent>`，矩阵持有字段继续默认 unique_ptr；业务来源改为统一 Provider，不改变现有删除方式。

保留旧候选校验、关闭守卫、许可等待、逻辑 Parent、CppOwnership、View 释放前 VM 存活及单次回收等行为，并通过原测试回归。本次不泛化全部弹窗接口或集合 Conductor。

### 4.4 兼容边界

这是对新 Provider 独占 API 的有意收缩。使用 OwnedInstance、get_deleter 或非虚析构基类独占映射的适配器／测试须同步调整并重建，不宣称该 API 源码或二进制 ABI 兼容。未配置 Provider 的旧应用、旧根工厂、默认 unique_ptr 的窗口／Conductor 调用继续保持源码与行为兼容。

## 5. 实施及验收

框架实现位于源码仓库，禁止修改下游 build/_deps 缓存副本。应用用独立构建目录选择待验证框架版本，记录实际 SHA；不在设计阶段改变依赖版本策略。

| 工作 | 验收 |
| --- | --- |
| IoC 公开入口及构建登记 | 普通 C++ 服务、名称、共享身份、独立创建、原错误转发；门面无具体容器依赖 |
| Bootstrapper 发布／解绑 | 配置失败、解析失败、启动异常、退出异常、清理异常、析构兜底和连续上下文；绑定冲突不影响前一实例 |
| 独占契约 | 具体非 QObject 类型及带虚析构接口支持默认删除；多重继承地址偏移正确；非虚析构基类到派生对象的独占映射编译期拒绝，共享测试保留 |
| 现有 Conductor 候选 | 沿用默认 unique_ptr；校验拒绝、守卫拒绝、延迟许可和成功接管行为不变 |
| 现有回收设施 | 不为 A1 扩展删除器能力；回归 View 借用、关闭／切换、管理者提前析构和失败装配的单次释放 |
| 兼容与业务接入 | 旧示例及独立消费工程原样构建启动退出；旧 unique_ptr、集合 Conductor、窗口、ViewHost、ViewLocator 回归；应用生产报到页和矩阵使用统一 Provider |

历史 F1 的 153 项和旧应用 F2 的 79/79 是原阶段证据，新 A1 必须记录自己的构建、测试、窗口启动退出和默认删除契约证据。麒麟、现场四屏及真实设备未验证时保持待验收，不用本机离屏测试替代。

## 6. 文档状态

[IoC 改造方案](IoC改造方案.md) 保留 F1／F2 实施史，A1 新增目标及所有权方案收缩由本文覆盖；[Bootstrapper](Bootstrapper.md)、[Conductor](Conductor.md)、[应用装配](IoC与应用装配.md) 当前接口说明保留，在对应位置说明当前框架增量及仍待实施的应用阶段。旧验收记录不改写为全局 IoC 已实现。


本轮实际实现和验证结果见 [A1框架IoC验收记录](验收/A1框架IoC验收记录.md)。应用 Provider 内置 Boost.DI、报到页与矩阵统一生产创建均不属于本轮完成范围。
