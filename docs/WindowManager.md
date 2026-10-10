# WindowManager 与模态弹窗

5A 已实现通用模态弹窗与 Home 重置确认，本机验证见 [5A 验收记录](5A验收记录.md)。5B 的回调守卫、void 请求接口迁移和 Detail 退出确认已实现并完成本机验收。

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

## 窗口挂载

具体 `WindowManager` 提供以下 C++ 接口，Bootstrapper 在根窗口加载成功后自动调用，独立 QML 窗口也可使用：

```cpp
bool attachToWindow(QQuickWindow *window, QQuickItem *fallbackFocusItem = nullptr);
void detachFromWindow();
```

挂载通过窗口自己的 QML 引擎创建标准 DialogHost，设置 CppOwnership、QObject 父对象及视觉父项为窗口场景根 contentItem。宿主填满场景区域，与业务布局并列，不参与页面布局；实际 Popup 使用 Overlay 展示。默认恢复有效的原焦点，无效时使用指定的后备焦点或窗口内容项。

同服务重复挂到同一窗口幂等成功；服务或目标窗口已有其他宿主、尚未完成的旧请求、非法线程、空窗口或窗口无 QML 引擎时返回 false 并诊断。后备焦点须属于同一窗口。切换窗口先 detachFromWindow；新挂载不替换现有手动宿主。两个独立服务可分别挂到两个独立窗口，但 Bootstrapper 仍只管理一个根窗口。

解除挂载会删除框架创建的宿主，先释放弹窗 View，再结束请求。窗口或引擎销毁时也清理宿主；服务析构完成剩余请求，不能留下未完成 Future。detachFromWindow 不删除调用方手动创建的宿主；独立窗口中自行创建宿主时，由调用方管理其寿命。Bootstrapper 始终自动挂载标准宿主。

## 所有权与完成顺序

showDialogAsync 按值消费候选 unique_ptr，拒绝也会回收候选，不影响正在展示的请求。只接纳无 QObject 父对象、无逻辑 Parent、未激活且与服务同线程的 Screen；请求者必须非空且同线程。错误线程的候选通过 deleteLater 在其所属线程回收，调用方负责该线程的事件循环和寿命。所有服务调用均由应用保证在主线程执行。

接纳后设置 QObject 父对象为 WindowManager 和 CppOwnership，保持逻辑 Parent 为空，同步初始化和激活，再装配 View。管理者以 QObject 父树持有弹窗 VM；确认 VM 通过 QPointer 借用服务，不形成 shared_ptr 环。Bootstrapper 在 Configure 前创建独立窗口服务，通过根工厂传给装配层；框架、Home 工厂和 Home 的 shared_ptr 持有同一实例，服务本身不设置 QObject 父对象，Shell 不保存或暴露该服务。独立使用无参 buildShell 时仍创建独立服务。

同一管理者只登记一个 DialogHost，且只允许一个当前请求。关闭中仍 busy，currentDialog 已为空；先关闭 Popup 并卸载 View，再同步关闭 VM、安排 deleteLater、清空请求，最后完成 QPromise。请求标识保护加载、失败和释放通知；旧通知、重复关闭或重复按钮操作不提交第二次完成。VM 意外销毁时不再调用其生命周期。

请求者销毁、页面停用取消、宿主窗口失效、宿主卸载和管理者析构均清理当前请求。宿主必须关联窗口才能接纳请求，避免无窗口时 Future 悬挂。Qt 对象失效通过 QPointer 保护，实际 View 回收先于 VM。

## QML 展示与输入

DialogHost 使用 Qt Quick Controls 的 Item 型 Popup，复用 ViewRegistry/ViewHost 创建带 typed viewModel 的 Item。缺失映射、加载失败、非 Item 和注入不匹配均结束请求并交付异常。

Popup 模态隔离底层鼠标和键盘输入，外部点击不关闭，Escape 交付空值。Tab/Shift+Tab 限制在弹窗内，确认框默认聚焦取消；Return、Enter 和 Space 只执行当前焦点按钮。关闭动画禁用，关闭后恢复仍有效、可见且启用的原焦点，否则使用 fallbackFocusItem；宿主销毁或窗口不可见时跳过恢复。

ConfirmationRequest 仅保存 title/message/confirmText/cancelText。ConfirmActionViewModel 只表达接受和取消，ConfirmActionView 只展示文案及调用操作，业务重置由 Home 决定。

## DialogHostState：公开 QML 宿主协调接口

DialogHostState 是 `Caliburn.Micro.Qt 1.0` 公开、可创建的 QML 弹窗宿主协调类型。应用导入模块后可以直接使用 `DialogHostState {}`；标准展示优先使用 DialogHost，自定义宿主可以使用该辅助类型。业务请求通过 IWindowManager 发起，宿主通过下列公开接口协调展示与释放。其头文件位于 src，不作为公开 C++ SDK 头文件提供；这不限制它的公开 QML 接口，与 [ViewHostState](ViewHost.md#4-借用与所有权) 的定位一致。

| QML 接口 | 契约 |
| --- | --- |
| available: bool | 可读写，默认 false；由宿主声明是否具备展示条件，标准 DialogHost 绑定是否关联窗口。相同值不通知；设为 false 时以空结果请求关闭当前弹窗，仍需完成宿主释放协议。 |
| manager: IWindowManager | 可读写，默认 null；借用服务，不接管服务或弹窗 VM 的所有权。当前只接受具体 WindowManager 及其子类，且一个管理者同时只关联一个宿主；不支持的服务或重复宿主输出诊断，属性归一为 null。替换或清空管理者时先取消旧请求并解除旧关联；同一管理者重复赋值无操作。 |
| model: ScreenViewModel | 只读，默认 null；投影管理者的 currentDialog。关闭阶段即为空，此时请求仍可能等待宿主释放。 |
| requestId: string | 只读，默认空字符串；投影当前请求标识，关闭等待释放期间仍有效，完成后为空。不要仅凭 model 为空判断 Future 已完成。 |
| failed(id: string, message: string) | 报告匹配请求的展示失败，通过 Future 交付 std::runtime_error；开始关闭流程，不代替 View 卸载和释放报告。 |
| dismiss(id: string) | 请求匹配弹窗无决定关闭，结果为空值；不代表明确取消 false，不创建新请求。 |
| released(id: string) | 宿主关闭并卸载 View 后报告释放完成；只处理匹配、关闭中且不处于启动阶段的请求，随后停用关闭 VM、安排延迟回收并完成 Future。提前释放报告忽略，不作为关闭请求。 |
| availableChanged() | available 实际变化时通知。 |
| managerChanged() | 管理者赋值处理结束或服务销毁时通知；被拒绝的赋值也可能通知，不保证属性值实际变化。 |
| modelChanged() | model 和 requestId 共用通知；管理者关联变化、服务销毁或管理者的 currentDialogChanged 到达时发出。应重新读取两个属性，不将通知次数等同于请求次数。 |
| hideRequested(id: string) | 管理者请求宿主隐藏对应弹窗。宿主按 id 匹配当前展示，关闭 Popup、卸载 View 后调用 released(id)。 |

三个方法在未关联管理者时无操作；旧请求标识、已完成请求标识不影响新请求，关闭中的重复 failed/dismiss 不覆盖已经确定的结果。所有赋值和方法调用由调用方保证在应用主线程执行；DialogHostState 不增加逐次线程检查。

自定义宿主的正常协议是：设置 manager 和 available → model 通知后按 requestId 装配 View → 收到 hideRequested(id) 后关闭展示并卸载 View → released(id)。标准 DialogHost 已实现此协议及模态输入、焦点恢复，应用通常无需手写它。QML 契约测试覆盖直接创建、默认值、属性读写权限、方法调用、服务关联、过期通知及释放时序。

## Home 接入与版本边界

Home.reset 设置 resetPending 后发起确认；canReset 要求计数大于零且没有待处理重置。仅接受结果且 Home 仍活动时执行 CounterService.reset；取消、无决定关闭或异常保留计数。普通停用和关闭取消该 Home 的请求，代次标识防止旧结果修改恢复后的页面。reset 仍是 QML 业务入口，没有另留同步绕过确认的 Home 方法。

ViewRegistry 自动提供确认视图的默认映射，应用可在冻结前显式注册替换视图；显式映射加载失败不会回退。Bootstrapper 在根窗口加载后自动挂载 DialogHost，Shell 不参与宿主装配。框架现在需要 Core/Qml/Quick/QuickControls2；不依赖业务模块或 Boost.Ext.DI。

CM 3.2 WPF 使用同步 [WindowManager.ShowDialog](https://github.com/Caliburn-Micro/Caliburn.Micro/blob/3.2.0/src/Caliburn.Micro.Platform/net40/WindowManager.cs)，本项目保留 QFuture/QPromise 是明确的 Qt 适配差异，不通过嵌套事件循环复刻阻塞返回。Screen 与 Conductor 生命周期仍同步；5B 已将请求接口迁移为 void，实际完成看生命周期通知，见 [关闭守卫](关闭守卫.md)。
