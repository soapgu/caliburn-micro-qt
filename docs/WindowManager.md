# WindowManager 与模态弹窗

5A 已实现通用模态弹窗与 Home 重置确认，本机验证见 [5A 验收记录](5A验收记录.md)。5B 的回调守卫、Conductor 请求接口迁移和 Detail 退出确认仍为规划中、未实施、未验证。

## C++ 接口与结果

```cpp
using DialogResult = std::optional<bool>;
QFuture<DialogResult> showDialogAsync(std::unique_ptr<ScreenViewModel> viewModel,
                                    QObject *requester);
void closeDialog(ScreenViewModel *viewModel, DialogResult result = std::nullopt);
void cancelDialogsFor(QObject *requester);
```

IWindowManager 是抽象 QObject 服务，WindowManager 提供具体实现，两者均不可由 QML 创建。只读 busy/currentDialog 属性及通知用于宿主展示。普通 ScreenViewModel 即可作为弹窗，不要求继承确认 VM；自定义 VM 注入窗口服务，通过 closeDialog(this, result) 提交结果，5A 不使用 Screen.tryClose 关闭弹窗。

true 表示接受，false 表示明确取消，空值表示 Escape、显式取消、请求者失效或宿主卸载等未作决定的关闭。无效参数通过 Future 交付 std::invalid_argument；忙、宿主不可用、缺少映射或 View 加载失败交付 std::runtime_error。生命周期异常同样交付至 Future，不跨越 Qt 信号边界。调用方应使用带 QObject 上下文的 then/onFailed，不在 UI 线程阻塞等待；QFuture.cancel 不作为关闭 UI 的协议。

## 所有权与完成顺序

showDialogAsync 按值消费候选 unique_ptr，拒绝也会回收候选，不影响正在展示的请求。只接纳无 QObject 父对象、无逻辑 Parent、未激活且与服务同线程的 Screen；请求者必须非空且同线程。错误线程的候选通过 deleteLater 在其所属线程回收，调用方负责该线程的事件循环和寿命。所有服务调用均由应用保证在主线程执行。

接纳后设置 QObject 父对象为 WindowManager 和 CppOwnership，保持逻辑 Parent 为空，同步初始化和激活，再装配 View。管理者以 QObject 父树持有弹窗 VM；确认 VM 通过 QPointer 借用服务，不形成 shared_ptr 环。应用每次 buildShell 创建独立窗口服务，由 Shell、Home 工厂和 Home 的 shared_ptr 持有，服务本身不设置 QObject 父对象。

同一管理者只登记一个 DialogHost，且只允许一个当前请求。关闭中仍 busy，currentDialog 已为空；先关闭 Popup 并卸载 View，再同步关闭 VM、安排 deleteLater、清空请求，最后完成 QPromise。请求标识保护加载、失败和释放通知；旧通知、重复关闭或重复按钮操作不提交第二次完成。VM 意外销毁时不再调用其生命周期。

请求者销毁、页面停用取消、宿主窗口失效、宿主卸载和管理者析构均清理当前请求。宿主必须关联窗口才能接纳请求，避免无窗口时 Future 悬挂。Qt 对象失效通过 QPointer 保护，实际 View 回收先于 VM。

## QML 展示与输入

DialogHost 使用 Qt Quick Controls 的 Item 型 Popup，复用 ViewRegistry/ViewHost 创建带 typed viewModel 的 Item。缺失映射、加载失败、非 Item 和注入不匹配均结束请求并交付异常。DialogHostState 是框架内部协调用的 QML 适配对象，不是业务入口。

Popup 模态隔离底层鼠标和键盘输入，外部点击不关闭，Escape 交付空值。Tab/Shift+Tab 限制在弹窗内，确认框默认聚焦取消；Return、Enter 和 Space 只执行当前焦点按钮。关闭动画禁用，关闭后恢复仍有效、可见且启用的原焦点，否则使用 fallbackFocusItem；宿主销毁或窗口不可见时跳过恢复。

ConfirmationRequest 仅保存 title/message/confirmText/cancelText。ConfirmActionViewModel 只表达接受和取消，ConfirmActionView 只展示文案及调用操作，业务重置由 Home 决定。

## Home 接入与版本边界

Home.reset 设置 resetPending 后发起确认；canReset 要求计数大于零且没有待处理重置。仅接受结果且 Home 仍活动时执行 CounterService.reset；取消、无决定关闭或异常保留计数。普通停用和关闭取消该 Home 的请求，代次标识防止旧结果修改恢复后的页面。reset 仍是 QML 业务入口，没有另留同步绕过确认的 Home 方法。

AppBootstrapper 在 ViewRegistry 冻结前登记确认映射，Shell 根窗口承载 DialogHost。框架现在需要 Core/Qml/Quick/QuickControls2；不依赖业务模块或 Boost.Ext.DI。

CM 3.2 WPF 使用同步 [WindowManager.ShowDialog](https://github.com/Caliburn-Micro/Caliburn.Micro/blob/3.2.0/src/Caliburn.Micro.Platform/net40/WindowManager.cs)，本项目保留 QFuture/QPromise 是明确的 Qt 适配差异，不通过嵌套事件循环复刻阻塞返回。Screen 和 Conductor 的同步生命周期与 bool 请求接口未改动；5B 仍按 [迭代计划](迭代实现计划.md#第五批阶段划分) 另行实施。
