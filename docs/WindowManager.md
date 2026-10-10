# WindowManager、普通窗口与独立模态窗口

窗口服务使用独立 QQuickWindow 承载 Item View，设置 Qt::Dialog、所属窗口 transientParent 和 Qt::ApplicationModal。保留 QFuture 异步返回，不引入 Widgets 或嵌套事件循环。验收状态见 [独立模态窗口验收记录](独立模态窗口验收记录.md)；历史记录保持原样。

## C++ 接口与结果

```cpp
using DialogResult = std::optional<bool>;
bool showWindow(const QVariant &viewModel);
QFuture<DialogResult> showDialogAsync(std::unique_ptr<ScreenViewModel> viewModel,
                                    QObject *requester);
void closeDialog(ScreenViewModel *viewModel, DialogResult result = std::nullopt);
void cancelDialogsFor(QObject *requester);
```

IWindowManager 是抽象 QObject 服务，WindowManager 提供具体实现，均不可由 QML 创建。普通 ScreenViewModel 即可作为弹窗；不要求继承确认 VM。busy 表示服务持有弹窗或正在清理，currentDialog 返回尚未进入最终清理的 VM，守卫等待和拒绝期间保持不变。

true 表示接受，false 表示明确取消；空值表示 Escape、标题栏关闭、无结果 tryClose 或强制取消。无效参数通过 Future 交付 std::invalid_argument；忙、所属窗口不可用、映射或展示失败交付 std::runtime_error。激活与关闭生命周期异常通过 Future 交付。调用方应使用带 QObject 上下文的 then/onFailed，不在 GUI 线程阻塞等待；QFuture.cancel 不作为关闭窗口的协议。

## 普通窗口职责与所有权

showWindow 借用一个同线程、无逻辑 Parent 的 ScreenViewModel，参数以 `QVariant::fromValue(具体类型指针)` 构造，保留具体类型供 QML required 属性注入。调用方持有 VM，服务设置 CppOwnership 并管理窗口资源；不会删除或接管普通窗口 VM。Bootstrapper 的 DisplayRootView 只委托此入口并记录结果。

单服务支持一个自建普通窗口。已有普通窗口或弹窗请求尚未结束时，在激活候选 VM 前拒绝。仅接纳本地或 qrc 映射，普通 View 必须直接为 QQuickWindow；不包装 Item。当前仍先激活 VM，再创建 QQmlApplicationEngine、注入、加载、登记所属窗口并创建 WindowConductor，最后显式 show，不要求 QML 自行设置 visible: true。打开普通窗口不占用 busy/currentDialog，普通窗口显示后可继续展示确认弹窗。

参数、映射和加载失败诊断后返回 false；加载失败立即释放部分创建资源并关闭已激活 VM，服务可以再次显示。激活异常清理后原样传播。关闭守卫及双向桥接沿用弹窗规则，实际窗口关闭后的普通 VM 停用由服务执行；已尝试关闭的生命周期不重复。关闭异常在 Qt 回调中捕获并发出 windowCleanupFailed，Bootstrapper 据此保留失败退出码。

成功显示后的普通窗口关闭仅隐藏窗口，窗口、View 与引擎保留到显式释放，供应用 OnExit 使用。调用方须让借用 VM 活到资源释放；VM 或窗口意外销毁时，在当前析构栈退出后排队清理剩余自建资源，旧清理随旧桥接销毁而取消。服务不保留对 Bootstrapper、根工厂或业务类型的依赖。

具体 WindowManager 提供两阶段清理：

```cpp
void prepareForShutdown(); // 停用桥接、结束弹窗，保留普通窗口及 View。
void releaseWindows();     // 释放自建窗口与引擎，兜底关闭仍未关闭的普通 VM。
```

所有操作在应用主线程执行。Bootstrapper 在 aboutToQuit 中准备退出、关闭根并执行 OnExit，在事件循环返回后显式释放窗口，最后删除根 VM；不依赖共享服务的析构时机。独立使用时须在删除 VM 前调用 releaseWindows；服务析构同样兜底释放。强制释放先销毁普通 View，再执行尚未尝试的 VM 关闭，不询问交互守卫。两阶段接口属于具体资源管理，不加入业务 IWindowManager 的请求协议。

根 VM/引擎/桥接拆分与验证见 [职责迁移验收](根窗口职责迁移验收记录.md)。本次对齐 CM 3.2.0 的 Bootstrapper → IWindowManager.ShowWindow 委托方向，创建/绑定/激活时序及通用 CreateWindow/EnsureWindow 仍留待独立变更。

## 内部窗口关联

`showWindow`、`showDialogAsync` 是公开展示入口。showWindow 在普通窗口加载后自动关联所属窗口、其 QML 引擎及后备焦点，不向业务内容树或 Overlay 插入宿主。

`attachToWindow`、`detachFromWindow` 为 private，只供 WindowManager 内部建立和解除关联；不支持外部窗口手动接入，也不支持在普通窗口继续显示时由调用方单独解除弹窗关联。窗口资源及其关联状态由同一服务维护。

不经过 Bootstrapper 时，同样先调用 showWindow 显示普通窗口，再通过 showDialogAsync 展示弹窗。切换普通窗口须先 releaseWindows，再调用 showWindow；不同服务分别管理各自创建的窗口。prepareForShutdown 是应用退出准备入口，执行后不能继续使用该窗口展示弹窗；恢复展示须释放旧窗口并重新创建。

退出准备、所属窗口或引擎销毁会强制清理弹窗并结束 Future，不询问交互守卫。普通窗口及引擎由服务持有，普通窗口 VM 仍由调用方持有。

## 展示与关闭守卫

每次请求创建内部 DialogWindow 与新的业务 View，沿用所属窗口的 QML 引擎、ViewRegistry 和 ViewHost，业务 View 仍须以 Item 为根并声明匹配的 typed viewModel。缺失资源、非 Item、语法错误或注入不匹配均结束请求并交付异常。初始窗口尺寸取业务 View 隐式尺寸加 20 像素内容边距，并限制在所属屏幕可用区域内。

closeDialog(vm, result)、Escape、标题栏关闭和无逻辑 Parent 的弹窗 tryClose 都经私有 WindowConductor 调用 canClose。原关闭事件立即拒绝，排队检查许可；许可通过后恢复关闭，跳过本桥接的重复询问，原有 QML onClosing 仍可拒绝。

等待及提交期间重复关闭不重新检查、不覆盖首次结果。守卫拒绝、抛异常或 QML 拒绝时保留窗口和 VM、丢弃本次结果，Future 保持未完成；以后可重新请求关闭。守卫异常在 Qt 边界记录。守卫按主线程回调一次的现有契约执行，不增加通用请求队列或代次协调。

直接完成 deactivate(true) 会反向请求关闭窗口并跳过守卫；普通停用不关闭窗口。已尝试的关闭生命周期不自动重试，已完成的关闭不再执行一次。其他关闭处理仍可拒绝窗口关闭；直接提交的 VM 状态不会回滚。

## 所有权与清理顺序

showDialogAsync 按值消费 unique_ptr，拒绝时同样回收候选。只接纳无 QObject 父对象、无逻辑 Parent、未激活且与服务同线程的 Screen；请求者须非空且同线程。错误线程候选通过 deleteLater 在所属线程回收，调用方负责其事件循环和寿命。

接纳后由 WindowManager 的 QObject 父树持有 VM，设置 CppOwnership，逻辑 Parent 保持为空，同步初始化和激活，再装配窗口。窗口同样由服务持有，transientParent 仅表达所属关系。确认 VM 通过 QPointer 借用服务，避免 shared_ptr 环。

实际关闭成功后先断开桥接并释放窗口父树，同步卸载、销毁 View/Loader；再执行一次 VM 关闭生命周期、安排 VM deleteLater，最后完成 Future。整个清理过程保持 busy。请求者销毁、cancelDialogsFor、解除关联、窗口/引擎失效及服务析构直接清理，不等待许可。VM 意外销毁时先完成其销毁通知，再结束窗口与 Future，不调用失效对象生命周期。外部直接销毁窗口时，等待窗口及子 View 析构完成后再执行 VM 生命周期。迟到许可通过桥接 QPointer 检查，不访问已销毁对象。

## 输入、焦点与业务接入

独立窗口以 ApplicationModal 限制应用其他窗口的用户输入；事件循环和程序调用继续执行。外部点击不关闭；Tab/Shift+Tab 在弹窗窗口内遍历，确认框默认聚焦取消。Escape 提交空结果，Return、Enter 和 Space 保持焦点控件的正常行为。

打开前保存所属窗口焦点，显示后激活弹窗并聚焦业务 View。正常关闭后排队激活所属窗口、恢复有效且可见启用的原焦点，否则使用后备焦点。已有新弹窗、强制清理、所属窗口不可见或销毁时跳过恢复。

ConfirmationRequest 保存文案，ConfirmActionViewModel 通过 closeDialog 提交接受或取消。确认视图默认映射由 ViewRegistry 提供，可在冻结前显式覆盖；覆盖加载失败不回退。Home 重置和 Detail 离开确认保留 Future 业务续接，只有 true 授权业务操作。

## 迁移与 CM 对齐边界

自定义实现 IWindowManager 的类须补充 showWindow；继承 WindowManager 的弹窗测试替身可沿用默认实现。普通窗口传入 typed QVariant 并借用 VM，不能复用 showDialogAsync 的 unique_ptr 接管约定。

旧 DialogHost、DialogHostState 及其 manager/available/requestId/failed/dismiss/released 手动宿主协议已移除。应用应删除旧 QML 宿主声明，由 Bootstrapper 委托 showWindow 创建窗口，或在独立用法中直接调用 showWindow。原公开 attachToWindow / detachFromWindow 已收为 private，外部窗口手动接入不再支持。内部 DialogWindow 不作为公开可创建的 QML 类型，测试和业务不得依赖其内部属性作为 SDK。

与 CM 3.2.0 对齐：独立窗口包装普通 View；窗口与 VM 统一守卫及双向关闭桥接。保留的适配差异是 QFuture 异步结果、Qt 对象所有权与延迟回收。单服务只允许一个弹窗；弹窗守卫中再次打开弹窗仍按忙状态失败，不支持嵌套模态窗口。系统强制退出与资源清理不等待交互许可。
