# IoC 与应用装配

当前 4C 已将 Shell 改为 Collection.OneActive，提供 Home/Detail 导航，两页共享计数服务，见 [4C 验收记录](4C验收记录.md)。4B 单项接入及更早装配方式的历史结果分别见 [4B 验收记录](4B验收记录.md) 与 [IoC 装配验收记录](IoC装配验收记录.md)。

## 4C 构造接口与模块边界

```cpp
explicit HomeViewModel(std::shared_ptr<CounterService> counterService);
explicit DetailViewModel(std::shared_ptr<CounterService> counterService);
using HomeViewModelFactory = std::function<std::unique_ptr<HomeViewModel>()>;
using DetailViewModelFactory = std::function<std::unique_ptr<DetailViewModel>()>;
explicit ShellViewModel(HomeViewModelFactory homeFactory,
                        DetailViewModelFactory detailFactory);
std::unique_ptr<ShellViewModel> buildShell();
```

CounterService 位于示例用户模块 services/，不注册为 QML 类型，没有 QObject 父对象。它唯一保存初始 0、范围 0～5 的计数，提供 count、canAdd、add、reset 和 countChanged；先比较剩余额度再相加，非法输入与无变化操作不通知。

Home 保留原计数属性、文案、操作及守卫，直接读取服务，只缓存上次守卫结果。构造按已有服务状态初始化缓存，不发初始通知；服务变化时先更新缓存，再通知 count 和实际变化的守卫。Detail 只读展示 count/message，使用同一服务并转发 countChanged。两者必须显式接收非空服务，空服务抛出 invalid_argument，没有隐式服务或无参构造。

Shell 继承 Conductor<ScreenViewModel>::Collection::OneActive，只保存两个工厂。home/detail 从集合查找，使用 itemsChanged 通知；activeItem 是实际选择，没有额外页面所有权。构造检查两个工厂非空，创建并选择未初始化 Home。空页面或接管失败抛出 invalid_argument，工厂自身异常原样传播。业务 VM 和框架不包含 DI 容器接口。

## 创建与所有权

[ViewModelComposition.cpp](../examples/minimal/app/ViewModelComposition.cpp) 每次 buildShell 创建独立 shared_ptr<CounterService>，装配 Home 和 Detail 工厂。两个工厂都仅按值捕获该服务，每次调用局部 Boost.Ext.DI 注入器，绑定同一服务并创建对应 unique_ptr 页面。根注入器绑定两个工厂、创建 Shell，返回根 unique_ptr。

DI 仍为装配库私有依赖。工厂不捕获注入器或 Shell；局部注入器离开作用域后，工厂和页面的 shared_ptr 保持服务寿命。Conductor 接管页面时设置 QObject 父关系和 CppOwnership，释放临时 unique_ptr。根 Shell 由 unique_ptr 管理，服务没有 QObject 父对象。不同 buildShell 的计数相互隔离。

旧 Detail 等待 DeferredDelete 时可以再次创建新 Detail，二者暂时共享服务。关闭 Shell 后重建 Home 仍沿用服务；父树提前销毁时同时回收当前成员及尚未删除的旧页，服务在最后一个 shared_ptr 释放后销毁一次。

## 生命周期与导航

| 操作 | 当前行为 |
| --- | --- |
| Shell 构造、initialize | 构造选择 Home；initialize 只初始化 Shell，不初始化页面。 |
| 首次激活 | Home 初始化、激活，最后 Shell 提交活动状态；初始化通知为 Shell 先、Home 后。 |
| showDetail | 仅活动且 Home 当前时可用；创建并选择 Detail，Home 普通停用且留在集合。 |
| Detail.goBack | Detail 内按钮调用 goBack，经 tryClose 和 IConductor 委托 Shell；Shell 先确保 Home 存在，再关闭 Detail、相邻选择回到 Home。 |
| 普通停用和恢复 | 保留集合和选择；若 Detail 当前，恢复后仍展示 Detail。 |
| 已初始化 Shell 关闭 | 清空整个集合及选择，关闭全部页面并延迟删除；未初始化关闭仍无操作。 |
| 关闭后激活 | 创建新 Home，沿用服务及计数。 |
| 页面意外销毁 | 移除成员；当前项失效则清空，不自动导航。下次 Shell 停用后激活补 Home、在空选择时选 Home。 |
| Home 缺失时返回 | 先补入 Home 再关闭 Detail；补入失败保留当前 Detail。 |
| 工厂失败 | 创建失败不提前改变选择，已有页面与业务状态保留。 |

Shell 重写 onActivate：补齐缺失 Home，没有选择时选择 Home，然后调用集合型基类。初始化、普通停用和关闭由基类传播。操作在应用主线程同步执行；生命周期钩子应正常返回，转换回调不得重入修改集合或销毁参与对象。

## View 与后续边界

ViewHost 继续绑定 activeItem。Home VM 导航期间常驻，但离开即卸载 Home View；返回创建新 View，文本和焦点按新 View 初始化，计数保留。Detail View 在 Detail VM 删除前卸载。Bootstrapper 登记 Shell/Home/Detail 映射，启动接口不变。

本轮验收见 [4C 验收记录](4C验收记录.md)，第四批本机验收完成，麒麟待验证。集合导航的 View 保留机制列入 [后续版本 ToDoList](后续版本ToDoList.md)，本轮没有实现缓存、异步确认或关闭守卫。

## 逻辑 Parent 与对象所有权

Parent 增量没有改变 buildShell、两个工厂或 CounterService 寿命。根 Shell 由 Bootstrapper 的 unique_ptr 持有，逻辑 parentViewModel 为空；Home/Detail 被接管后 QObject 父对象和逻辑 Parent 均为 Shell。关闭页面先清空逻辑 Parent，QObject 父关系保留到实际回收。

CounterService 无 QObject 父对象，由两个工厂及页面的 shared_ptr 持有，不属于逻辑 VM 树。Parent 不是 DI 容器或服务定位入口；业务和框架仍不依赖 DI。接口与验收见 [Conductor](Conductor.md) 和 [Parent 体系验收记录](Parent体系验收记录.md)。

## Detail 自关闭返回

Detail 新增 Q_INVOKABLE bool goBack()，不依赖 Shell、Home、页面工厂或 View。非活动时返回 false，活动时调用继承的 C++ tryClose。异常在 goBack 边界被捕获，输出“Detail 返回失败”并返回 false；不二次关闭或回滚生命周期。直接调用 C++ tryClose 时，异常仍向调用方传播。

DetailView 的 goBack 按钮绑定页面活动状态与非空逻辑 Parent，直接调用该页面 VM。ShellView 顶部只保留查看详情按钮；homeHost 和生命周期文字保持原标识。按钮执行后不再访问已卸载的 Detail View。Detail 显式配置页面与返回按钮间的 Tab / Shift+Tab 导航，避免依赖平台默认按钮 Tab 策略。

Shell 新增 deactivateItem(ViewModelBase*, bool) override。仅当 close=true 且目标为当前 Detail 时先 ensureHome，再通过 ConductorCollectionOneActiveViewModelBase::deactivateItem 完成关闭，避免递归。其他目标和普通停用直接委托基类。Home 工厂空返回、接管拒绝或抛异常时，Detail 尚未被关闭，其 Parent、选择和生命周期保留；恢复工厂后可以重试。

正常返回保留 Home VM 身份和服务计数；Home 缺失时创建新 Home，仍读取原服务。buildShell、DI 捕获边界、页面所有权及 CounterService 寿命没有改变。本轮结果见 [tryClose 验收记录](tryClose验收记录.md)，第四批历史记录保持原样。
