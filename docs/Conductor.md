# Conductor：单项与 Collection.OneActive

前四批及 5B 已实现并完成本机验收，见 [5B 验收记录](5B验收记录.md)。本轮 CM 精简已完成本机复验，见 [精简记录](CM关闭守卫精简验收记录.md)。历史第四批、Parent 与 tryClose 验收记录保留原样；麒麟待验证。

## 统一协议与逻辑 Parent

IChild、IParent、IConductor 均为带虚析构的纯 C++ 接口。IID 分别为 `Caliburn.Micro.Qt.IChild/1.0`、`Caliburn.Micro.Qt.IParent/1.0`、`Caliburn.Micro.Qt.IConductor/2.0`。5B 将 IConductor 请求返回值改为 void，消费工程须重新编译。

```cpp
virtual void activateItem(ViewModelBase *item) = 0;
virtual void deactivateItem(ViewModelBase *item, bool close) = 0;
```

ConductorBase 继承 ScreenViewModel，声明 Q_INTERFACES(IParent IConductor)，提供 activationProcessed 信号、关闭策略及直接许可回调。具体元对象基类向 QML 暴露只读 activeItem；集合型另有只读 items。模板仅提供 C++ 类型约束，无独立元对象。

Screen 的 parentViewModel 为只读逻辑 Parent，使用 QPointer 保存，与 QObject 父树分开。Conductor 接管时建立 QObject 所有权及逻辑 Parent；单项普通停用和集合切换保留 Parent，关闭时先清空逻辑 Parent，QObject 父关系保留到回收。普通 VM 可自行实现 IChild，服务不属于逻辑 VM 树。

## 单项类型与使用

Conductor<T> 的 T 必须是非 const/volatile 的 ViewModelBase 派生类，默认 ViewModelBase。只有 Screen 执行生命周期。

```cpp
Conductor<ScreenViewModel> conductor;
conductor.activate();
auto owned = std::make_unique<MyScreen>();
auto *page = owned.get();
conductor.activateItem(std::move(owned));
// 立即许可可同步完成；延后许可须通过信号观察完成，不能根据方法返回推断。
conductor.deactivateItem(page, false);
conductor.activateItem(page);
conductor.closeItem(page);
```

上例要求各请求立即完成，真实延后守卫应由界面操作或完成信号接续。请求均为普通 C++ void 方法，不是 Q_INVOKABLE，不带完成回调，不提供另一套 Async 入口。

| 操作 | 许可及提交行为 |
| --- | --- |
| activateItem(unique_ptr<U>&&) | 基础校验后暂存候选；旧当前项许可通过才接管新项并关闭旧项。 |
| activateItem(T*) | 选择本 Conductor 已接管、仍有效且未关闭的对象；替换当前项前检查旧项许可。 |
| activateItem(nullptr) | 请求关闭当前项；没有当前项时无操作。 |
| deactivateItem(item, false) | 仅处理当前项并检查许可；同意后清空选择，普通停用，保留 QObject 所有权和 Parent。 |
| deactivateItem(item, true) / closeItem(item) | 只处理当前项；检查许可后清空选择和 Parent、关闭并 deleteLater；closeItem 经 IConductor 虚调用委托。 |
| getChildren() | 只枚举当前项。 |

内部只记录当前项及其销毁通知。父级 canClose 和生命周期只处理当前项；普通停用后的旧对象不再登记为受管成员，但业务可重新选择仍有效且未关闭的旧对象。旧对象保留父关系，由 QObject 父树兜底回收；非当前旧对象的停用/关闭请求无操作。已提交关闭的对象不得再使用，没有跨 Conductor 转移接口。

## Collection.OneActive

ConductorCollectionOneActive<T> 也可写作 Conductor<T>::Collection::OneActive；独立实现集合算法，不复用单项切换关闭路径。items/getChildren 按加入顺序返回借用快照，修改快照不会修改集合。

| 操作 | 行为 |
| --- | --- |
| bool addItem(unique_ptr<U>&&) | 同步校验并接管，添加但不选择、不初始化；拒绝不移动 unique_ptr。 |
| void activateItem(unique_ptr<U>&&) | 接管、加入并选择；旧项普通停用，保留成员，不检查关闭守卫。 |
| void activateItem(T*) | 选择已有成员，外部对象拒绝；切换不检查关闭守卫。 |
| void activateItem(nullptr) | 清空选择，普通停用旧项，集合保留。 |
| void deactivateItem(T*, false) | 普通停用目标，保留成员、Parent 和选择，不检查守卫。 |
| void deactivateItem(T*, true) / closeItem(T*) | 许可通过后移除并关闭；当前项优先选前一项，否则后一项。 |

无效停用目标和已关闭对象无操作；非空激活目标被拒绝时发送 activationProcessed(item, false)。普通 VM 没有 Screen 生命周期动作。

## 接管、通知与回收

新对象须无 QObject 父对象、同线程、不是自身或祖先、Screen 尚未活动，且逻辑 Parent 为空。基础校验失败不移动调用方 unique_ptr；通过后单项以回调捕获的标准 RAII 容器持有候选，拒绝或回调释放时回收，许可通过前不加入成员或建立 Parent。等待期间禁止外部删除候选；不提供候选提前销毁保护或管理者销毁时立即回收的保证。

提交继续沿用先成员/选择、再生命周期的顺序：单项 A → B 先接管 B、提交记录和选择、清空 A Parent、设置 B Parent，再通知选择、关闭 A、按父活动状态激活 B、安排 A.deleteLater、发布激活结果。集合加入并选择先更新 Parent，再依次通知 itemsChanged、activeItemChanged；关闭当前项先移除及选择相邻项，再通知、执行生命周期及延迟删除。关闭时旧 View 先于 VM 卸载。

当前项意外销毁清空选择；集合成员销毁清理成员记录，不自动导航。QObject 父树兜底回收旧对象和等待删除对象。QPointer 只保护延后回调的借用对象寿命，不实现请求代次或过期协调。

## 父生命周期

父 activate 只激活当前 Screen；initialize 不初始化子项。父 deactivate(false) 同步停用当前项，保留选择及全部成员，不增加许可检查。已初始化父 deactivate(true) 清空选择和受管记录、清空 Parent、通知并关闭及安排删除；单项仅处理当前项，集合处理全部成员。未初始化父关闭仍无操作。

父级 canClose 检查 getChildren 快照并交付许可：单项只检查当前项，集合检查全部成员，递归支持嵌套 Conductor，不自行关闭成员。任一拒绝时整体拒绝，不移除已同意的成员。直接生命周期用于执行与强制清理，不能代替受管成员请求；Bootstrapper 的退出清理不等待守卫。

## 激活与关闭完成通知

activationProcessed(item, success) 保留既有规则：非空新选择及有效恢复完成时成功，失败时 false；父活动时重复同项激活成功，非活动时重复同项无通知。nullptr、添加、普通停用及父生命周期传播不发送；集合关闭当前项并自动选择非空相邻项时发布新项激活成功。

Screen.attemptingDeactivation(close) 在有效停用执行前发出；deactivated(close) 在同步钩子正常结束后发出，close=true 表示关闭生命周期完成，实际删除仍可延后。拒绝或失效不发送成功关闭通知，未初始化对象不伪造生命周期通知。普通 VM 的成员变化通过 items/activeItem/Parent 观察。

## Screen.tryClose 与公共协议

void tryClose() 每次通过逻辑 Parent 获取 IConductor，调用 deactivateItem(this, true)，不使用 QObject 父对象兜底。没有 Parent 或接口不匹配时无操作。Detail.goBack 是 QML void 入口，仅活动时发起请求，完成依靠生命周期及页面状态观察。

集合型 prepareCloseItem 是受保护虚钩子，仅在许可通过后、关闭提交前调用。Shell 在当前 Detail 关闭时补齐缺失 Home；取消不补建，补建失败保留 Detail。基类在准备后重新校验成员，再按相邻项规则关闭。此业务策略不进入通用算法。

## 5B 关闭守卫与回调式请求规划

本节标题保留旧锚点，当前能力已经实现。完整接口、直接回调、使用约定、异常与 CM 3.2.0 对齐差异见 [关闭守卫](关闭守卫.md)，验收见 [5B 记录](5B验收记录.md)。根窗口请求已由 [Bootstrapper](Bootstrapper.md) 接入守卫；Action 与 View 缓存继续见 [后续事项](后续版本ToDoList.md)。

所有操作及守卫回调由应用保证在主线程执行；同步转换、钩子和通知观察者不得重入修改参与对象或将其销毁。守卫必须回调一次，等待期间不支持重叠请求、修改参与成员或替换策略。没有 Request、取消上下文、去重或代次保护。守卫、策略和生命周期异常直接传播且不回滚；异步调用方在自己的 Qt 边界处理异常。
