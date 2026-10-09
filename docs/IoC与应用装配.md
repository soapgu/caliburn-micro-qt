# IoC 与应用装配

当前 4C 已将 Shell 改为 Collection.OneActive，提供 Home/Detail 导航，两页共享计数服务，见 [4C 验收记录](4C验收记录.md)。4B 单项接入及更早装配方式的历史结果分别见 [4B 验收记录](4B验收记录.md) 与 [IoC 装配验收记录](IoC装配验收记录.md)。

## 4C 构造接口与模块边界

以下为窗口服务与宿主归入框架后的当前签名；4C 原始签名保留在历史验收记录中。

```cpp
explicit HomeViewModel(std::shared_ptr<CounterService> counterService,
                       std::shared_ptr<IWindowManager> windowManager);
explicit DetailViewModel(std::shared_ptr<CounterService> counterService);
using HomeViewModelFactory = std::function<std::unique_ptr<HomeViewModel>()>;
using DetailViewModelFactory = std::function<std::unique_ptr<DetailViewModel>()>;
explicit ShellViewModel(HomeViewModelFactory homeFactory,
                        DetailViewModelFactory detailFactory);
std::unique_ptr<ShellViewModel> buildShell();
std::unique_ptr<ShellViewModel> buildShell(std::shared_ptr<IWindowManager> windowManager);
```

CounterService 位于示例用户模块 services/，不注册为 QML 类型，没有 QObject 父对象。它唯一保存初始 0、范围 0～5 的计数，提供 count、canAdd、add、reset 和 countChanged；先比较剩余额度再相加，非法输入与无变化操作不通知。

Home 保留原计数属性、文案和操作，计数直接读取服务，缓存上次可用条件并维护重置请求的 pending/代次状态。构造按已有服务状态初始化缓存，不发初始通知；服务变化时先更新缓存，再通知 count 和实际变化的守卫。Detail 只读展示 count/message，使用同一服务并转发 countChanged。两者必须显式接收非空服务，空服务抛出 invalid_argument，没有隐式服务或无参构造。

Shell 继承 Conductor<ScreenViewModel>::Collection::OneActive，保存两个页面工厂。home/detail 从集合查找，使用 itemsChanged 通知；activeItem 是实际选择，没有额外页面所有权。构造检查两个工厂非空，创建并选择未初始化 Home。空页面或接管失败抛出 invalid_argument，工厂自身异常原样传播。业务 VM 和框架不包含 DI 容器接口。

## 创建与所有权

[ViewModelComposition.cpp](../examples/minimal/app/ViewModelComposition.cpp) 在应用启动时接收 Bootstrapper 创建的 WindowManager，每次 buildShell 创建独立 CounterService，以 shared_ptr 持有并装配 Home/Detail 工厂。无参 buildShell 供独立装配使用，自行创建窗口服务；调用方若需要弹窗，应使用接收服务的重载，显式保留服务并调用其挂载接口。Home 工厂按值捕获计数和窗口服务，Detail 工厂仅捕获计数服务，每次调用局部 Boost.Ext.DI 注入器，绑定同一服务并创建对应 unique_ptr 页面。根注入器只绑定两个页面工厂、创建 Shell，返回根 unique_ptr。

DI 仍为装配库私有依赖。工厂不捕获注入器或 Shell；局部注入器离开作用域后，工厂和页面的 shared_ptr 保持服务寿命。Conductor 接管页面时设置 QObject 父关系和 CppOwnership，释放临时 unique_ptr。根 Shell 由 unique_ptr 管理，服务没有 QObject 父对象。不同 buildShell 的计数相互隔离。

旧 Detail 等待 DeferredDelete 时可以再次创建新 Detail，二者暂时共享服务。关闭 Shell 后重建 Home 仍沿用服务；父树提前销毁时同时回收当前成员及尚未删除的旧页，服务在最后一个 shared_ptr 释放后销毁一次。

## 生命周期与导航

| 操作 | 当前行为 |
| --- | --- |
| Shell 构造、initialize | 构造选择 Home；initialize 只初始化 Shell，不初始化页面。 |
| 首次激活 | 框架先提交 Shell 初始化和活动状态，再通过 Conductor 完成 Home 初始化与激活；两种状态通知均为父先、子后。 |
| showDetail | 仅活动且 Home 当前时可用；创建并选择 Detail，Home 普通停用且留在集合。 |
| Detail.goBack | Detail 内按钮调用 goBack，经 tryClose 和 IConductor 委托 Shell；Shell 先确保 Home 存在，再关闭 Detail、相邻选择回到 Home。 |
| 普通停用和恢复 | 保留集合和选择；若 Detail 当前，恢复后仍展示 Detail。 |
| 已初始化 Shell 关闭 | 清空整个集合及选择，关闭全部页面并延迟删除；未初始化关闭仍无操作。 |
| 关闭后激活 | 创建新 Home，沿用服务及计数。 |
| 页面意外销毁 | 移除成员；当前项失效则清空，不自动导航。下次 Shell 停用后激活补 Home、在空选择时选 Home。 |
| Home 缺失时返回 | 先补入 Home 再关闭 Detail；补入失败保留当前 Detail。 |
| 工厂失败 | 创建失败不提前改变选择，已有页面与计数保留；Shell 激活钩子重建失败时活动状态已提交，不回滚，恢复工厂后需先停用再激活。 |

Shell 重写 onActivate：补齐缺失 Home，已有选择时调用集合型基类激活当前项；没有选择时通过 activateItem 选择 Home 并由集合入口完成激活，随后直接返回，不再重复调用基类激活同一项。这是页面选择与重建逻辑，不操作 Shell 自身生命周期。初始化、普通停用和关闭由框架管理。操作在应用主线程同步执行；生命周期钩子应正常返回，转换回调不得重入修改生命周期、集合或销毁参与对象。

Bootstrapper 在根 QML 加载前只调用 Shell.activate()，不额外调用 initialize；首次初始化由 Screen 自动完成。Home/Detail 不在钩子中自行激活或关闭。状态通知先于钩子执行，不表示整个父子转换完成；钩子抛异常时已提交状态不回滚。

## View 与后续边界

ViewHost 继续绑定 activeItem。Home VM 导航期间常驻，但离开即卸载 Home View；返回创建新 View，文本和焦点按新 View 初始化，计数保留。Detail View 在 Detail VM 删除前卸载。Bootstrapper 登记 Shell/Home/Detail 映射，启动接口不变。

本轮验收见 [4C 验收记录](4C验收记录.md)，第四批本机验收完成，麒麟待验证。集合导航的 View 保留机制列入 [后续版本 ToDoList](后续版本ToDoList.md)，4C 当时没有实现缓存、异步确认或关闭守卫；5A 已增加重置确认，缓存和关闭守卫仍未实现。

## 逻辑 Parent 与对象所有权

Parent 增量没有改变 buildShell、两个工厂或 CounterService 寿命。根 Shell 由 Bootstrapper 的 unique_ptr 持有，逻辑 parentViewModel 为空；Home/Detail 被接管后 QObject 父对象和逻辑 Parent 均为 Shell。关闭页面先清空逻辑 Parent，QObject 父关系保留到实际回收。

CounterService 无 QObject 父对象，由两个工厂及页面的 shared_ptr 持有，不属于逻辑 VM 树。Parent 不是 DI 容器或服务定位入口；业务和框架仍不依赖 DI。接口与验收见 [Conductor](Conductor.md) 和 [Parent 体系验收记录](Parent体系验收记录.md)。

## Detail 自关闭返回

Detail 新增 Q_INVOKABLE bool goBack()，不依赖 Shell、Home、页面工厂或 View。非活动时返回 false，活动时调用继承的 C++ tryClose。异常在 goBack 边界被捕获，输出“Detail 返回失败”并返回 false；不二次关闭或回滚生命周期。直接调用 C++ tryClose 时，异常仍向调用方传播。

DetailView 的 goBack 按钮绑定页面活动状态与非空逻辑 Parent，直接调用该页面 VM。ShellView 顶部只保留查看详情按钮；homeHost 和生命周期文字保持原标识。按钮执行后不再访问已卸载的 Detail View。Detail 显式配置页面与返回按钮间的 Tab / Shift+Tab 导航，避免依赖平台默认按钮 Tab 策略。

Shell 新增 deactivateItem(ViewModelBase*, bool) override。仅当 close=true 且目标为当前 Detail 时先 ensureHome，再通过 ConductorCollectionOneActiveViewModelBase::deactivateItem 完成关闭，避免递归。其他目标和普通停用直接委托基类。Home 工厂空返回、接管拒绝或抛异常时，Detail 尚未被关闭，其 Parent、选择和生命周期保留；恢复工厂后可以重试。

正常返回保留 Home VM 身份和服务计数；Home 缺失时创建新 Home，仍读取原服务。buildShell、DI 捕获边界、页面所有权及 CounterService 寿命没有改变。本轮结果见 [tryClose 验收记录](tryClose验收记录.md)，第四批历史记录保持原样。

## 第五批装配与确认

**5A 已实现并完成本机验收；5B 规划中、未实施、未验证。** 窗口服务及宿主现由框架统一创建管理，装配层将框架提供的 IWindowManager 注入 Home，确认 VM/View 使用框架默认映射；Shell 不声明 DialogHost 或服务接线属性。Home 用 showDialogAsync 通用模态弹窗入口完成重置确认。ConfirmationRequest 只保存确认文案，实际重置仍由 Home 执行；入口同时支持自定义弹窗 VM，以 QFuture / QPromise 交付结果。

5B 复用 5A 的窗口管理能力，由 Detail 的 canClose(callback) 守卫消费窗口 Future 并回传许可，再由 Conductor 决定是否关闭，不在返回按钮操作中单独提前确认。未来 tryClose 等入口按 CM 3.2 迁移为普通命名的 void 请求方法，goBack 不能再依赖同步 bool 或方法返回推断关闭完成；当前代码调用链保持现状，迁移与完成通知接续属于 5B。

Shell 未来须在许可通过后、实际关闭提交前 ensureHome；用户取消时不能因提前补建 Home 改变集合，补建失败时保留 Detail。此调整不放入通用集合算法。具体构造签名、回调类型、工厂捕获、完成通知和所有权细节待实施前设计。以上 Shell 关闭时机与回调迁移仍是 5B 规划，5A 未实施这些调整，完整范围见 [第五批阶段划分](迭代实现计划.md#第五批阶段划分)。

5A 窗口服务、弹窗所有权及 Home 的 pending/代次保护见 [WindowManager](WindowManager.md)，本机证据见 [5A 验收记录](5A验收记录.md)。
