# WindowManager、普通窗口与独立模态窗口

窗口服务使用独立 QQuickWindow 承载 Item View，设置 Qt::Dialog、所属窗口 transientParent 和 Qt::ApplicationModal。保留 QFuture 异步返回，不引入 Widgets 或嵌套事件循环。验收状态见 [独立模态窗口验收记录](验收/独立模态窗口验收记录.md)；历史记录保持原样。

## C++ 接口与结果

```cpp
using DialogResult = std::optional<bool>;
bool showWindow(const QVariant &viewModel,
                const WindowOptions &options = {},
                const std::optional<QString> &context = std::nullopt);
QQuickWindow *windowFor(QObject *viewModel) const;
// 信号参数为 VM；只在普通窗口实际关闭后发送。
void windowClosed(QObject *viewModel);
QFuture<DialogResult> showDialogAsync(std::unique_ptr<QObject> viewModel,
                                    QObject *requester,
                                    const DialogOptions &options = {},
                                    const std::optional<QString> &context = std::nullopt);
```

IWindowManager 是抽象 QObject 服务，WindowManager 提供具体实现，均不可由 QML 创建。模型允许普通 QObject，不要求继承 Screen 或确认 VM。currentDialog 返回 QObject*。DialogResult 定义于独立公开头文件 DialogResult.h。busy 表示服务持有弹窗或正在清理，currentDialog 返回尚未进入最终清理的 VM，守卫等待和拒绝期间保持不变。

true 表示接受，false 表示明确取消；空值表示 Escape、标题栏关闭、无结果 tryClose 或强制取消。无效参数通过 Future 交付 std::invalid_argument；忙、所属窗口不可用、映射或展示失败交付 std::runtime_error。激活与关闭生命周期异常通过 Future 交付。调用方应使用带 QObject 上下文的 then/onFailed，不在 GUI 线程阻塞等待；QFuture.cancel 不作为关闭窗口的协议。

## 可选能力与主动关闭

窗口分别查询 IActivate、IDeactivate、IGuardClose、IChild：缺少激活或停用接口时跳过生命周期，缺少守卫时默认允许关闭，缺少 IChild 时不维护逻辑 Parent。QObject 父关系仍单独决定所有权。

窗口通过 IDeactivate 的带 QObject 上下文订阅方法接收尝试停用和完成通知。deactivated(true) 反向关闭窗口，deactivated(false) 不关窗；尝试关闭后不自动重试生命周期，即使钩子抛异常。

Screen.tryClose(result) 优先委托逻辑 Parent Conductor，否则通过内部窗口弱关联直接调用 close()；closeRequested 已移除。普通窗口忽略结果，内部弹窗通过 dialogResult 属性暂存 bool 或空值，由首次关闭事件固定本次结果。普通 QObject 仍可通过标题栏、Escape 关闭并返回空结果；实现 IDeactivate 后还可通过关闭完成通知反向关窗，该路径跳过守卫并返回空结果。不新增公开关闭请求接口。

## 普通窗口职责与所有权

showWindow 借用一个同线程、无逻辑 Parent 的 QObject，参数以 `QVariant::fromValue(具体类型指针)` 构造，保留具体类型供 QML required 属性注入。调用方持有 VM，服务设置 CppOwnership 并管理窗口资源；不会删除或接管普通窗口 VM。Bootstrapper 的 displayRootView 只委托此入口并记录结果。

单服务支持一个主窗和多个辅助窗。WindowOptions.role 默认为 WindowRole::Primary，重复主窗、弹窗请求未结束或 VM 已关联窗口时拒绝。WindowRole::Auxiliary 要求已有可用主窗；辅助窗独立创建引擎、桥接和原生窗口，不设置 parent/transientParent，不作为弹窗所属窗口。仅接纳本地或 qrc 映射，普通 View 必须直接为 QQuickWindow，不包装 Item。服务创建 QQmlApplicationEngine、注入 typed 模型和初始 visible=false、加载并校验 View、登记所属窗口并建立关闭桥接，然后调用 create() 创建隐藏的原生窗口资源，再激活模型，最后显式 show。普通窗口不占用 busy/currentDialog；根激活钩子中可以使用已关联的所属窗口打开弹窗。

参数、映射和加载失败诊断后返回 false；加载失败不激活模型，立即释放部分创建资源，服务可再次显示。激活异常先解除桥接并销毁 View，再对已经进入展示生命周期的模型执行一次关闭收尾，原异常继续传播。实际普通窗口关闭后的停用由服务执行，已尝试关闭的生命周期不重复；关闭异常在 Qt 回调中捕获并发出 windowCleanupFailed，Bootstrapper 据此保留失败退出码。

主窗成功关闭后，强制清理辅助窗和在途弹窗，不再询问辅助窗守卫；主窗、View 与引擎保留到显式释放，供应用 onExit 使用。辅助窗成功关闭仅收尾自身，排队释放其 View/引擎，不影响主窗或其他辅助窗，也不自动重建。窗口关闭生命周期执行后发送 windowClosed(viewModel)，VM 若在钩子中销毁则参数为空。强制销毁/退出释放不发送此正常关闭信号。调用方须让借用 VM 活到资源释放；VM 或窗口意外销毁时，在当前析构栈退出后排队清理剩余自建资源，旧清理随旧桥接销毁而取消。服务不保留对 Bootstrapper、根工厂或业务类型的依赖。

具体 WindowManager 提供两阶段清理：

```cpp
void prepareForShutdown(); // 停用桥接、结束弹窗，保留普通窗口及 View。
void releaseWindows();     // 释放自建窗口与引擎，兜底关闭仍未关闭的普通 VM。
```

所有操作在应用主线程执行。Bootstrapper 在 aboutToQuit 中准备退出、关闭根并执行 onExit，在事件循环返回后显式释放窗口，最后删除根 VM；不依赖共享服务的析构时机。独立使用时须在删除 VM 前调用 releaseWindows；服务析构同样兜底释放。强制释放先销毁普通 View，再执行尚未尝试的 VM 关闭，不询问交互守卫。两阶段接口属于具体资源管理，不加入业务 IWindowManager 的请求协议。

根 VM/引擎/桥接拆分与历史验证见 [职责迁移验收](验收/根窗口职责迁移验收记录.md)。当前已对齐 CM 3.2.0 的创建、绑定、激活、显示顺序及直接窗口关闭路径，见 [窗口直接关闭迁移](tryClose窗口直接关闭.md)；普通 Item 的通用窗口包装仍未支持。

## 窗口配置与诊断

WindowOptions 定义在公开头文件 WindowOptions.h：role 默认为 Primary；screen 是 QPointer<QScreen> 弱引用；geometry、flags、visibility 为可选覆盖；activateOnShow 默认 false。未指定的配置沿用 QML/Qt 行为。几何使用虚拟桌面逻辑坐标，setScreen 本身不会自动移动窗口。

服务在创建原生资源前应用屏幕、flags 和几何，在激活后应用显示状态并按需请求激活。辅助窗固定为独立非模态顶层窗口。框架不内置四屏尺寸、X11 flags 或 DPI 策略，这些由应用决定。

windowFor 在服务线程返回借用的 QQuickWindow*；VM 未关联或调用线程错误时返回 nullptr。调用方使用 QPointer 观察失效，仅在启动诊断/测试层读取几何、连接渲染信号，业务 VM 继续使用 tryClose。公开配置类型依赖 QtGui，框架 CMake 目标公开传递 Qt6::Gui。

弹窗使用独立公开类型 `DialogOptions`（`<CaliburnMicroQt/DialogOptions.h>`），包含 `QString title`（默认空字符串）和 `QColor backgroundColor`（默认无效颜色）。`showDialogAsync` 的第三个参数为展示选项、第四个参数为独立 context，两者均可省略；原有 (model, requester, options) 调用及 `DialogOptions{title}` 写法不变，消费工程需重新编译。标题在业务组件完成创建前设置，不从 VM 的 title 属性推断，也不改变正文标题。标题只在展示时复制一次，不提供动态绑定。弹窗尺寸、flags、屏幕及模态继续由框架管理，不复用 WindowOptions。

最小示例 HomeViewModel::reset 显式传入原生标题：

```cpp
DialogOptions options;
options.title = QStringLiteral("重置计数");
auto future = windows->showDialogAsync(std::move(dialog), this, options);
```

`backgroundColor` 未指定或为无效颜色时，弹窗客户区使用系统 `QPalette::Window`，并随系统调色板变化更新；指定有效颜色时，在业务视图加载前应用，并在本次弹窗存续期间保持该颜色。该选项不修改业务 View 的背景，调用方可传入与业务主题一致的颜色，统一现有 20 像素留白与内容区域的外观。原生标题栏由系统绘制，不受此选项控制。

```cpp
DialogOptions options{QStringLiteral("人员管理")};
options.backgroundColor = QColor(QStringLiteral("#FFFFFF"));
auto future = windows->showDialogAsync(std::move(dialog), this, options);
```

普通窗口关闭后发送 windowClosed(QObject *viewModel)，参数是 VM 而非窗口。Bootstrapper 仅处理根 VM 的关闭通知，辅助窗关闭不会触发应用退出。框架不为普通窗口自动绑定 Esc；既有模态弹窗 Escape 行为保持不变。

## 内部窗口关联

`showWindow`、`showDialogAsync` 是公开展示入口。showWindow 在主窗加载后自动关联弹窗所属窗口、其 QML 引擎及后备焦点，不向业务内容树或 Overlay 插入宿主。

`attachToWindow`、`detachFromWindow` 为 private，只供 WindowManager 内部建立和解除关联；不支持外部窗口手动接入，也不支持在普通窗口继续显示时由调用方单独解除弹窗关联。窗口资源及其关联状态由同一服务维护。

不经过 Bootstrapper 时，同样先调用 showWindow 显示普通窗口，再通过 showDialogAsync 展示弹窗。替换主窗须先 releaseWindows，再调用 showWindow；新增辅助窗使用 Auxiliary 角色，辅助窗候选加载或激活失败只清理该候选。prepareForShutdown 是应用退出准备入口，执行后不能继续使用该窗口展示弹窗；恢复展示须释放旧窗口并重新创建。

退出准备、所属窗口或引擎销毁会强制清理弹窗并结束 Future，不询问交互守卫。普通窗口及引擎由服务持有，普通窗口 VM 仍由调用方持有。

## 展示与关闭守卫

每次请求由 C++ 创建私有 DialogWindow（QQuickWindow）与新的业务 View，不再加载 DialogWindow.qml；沿用所属窗口的 QML 引擎、ViewLocator 和 ViewHost，业务 View 仍须以 Item 为根并声明匹配的 typed viewModel。缺失资源、非 Item、语法错误或注入不匹配均结束请求并交付异常。初始窗口尺寸取业务 View 隐式尺寸（未提供时回退为 360 × 220）加四周各 20 像素内容边距，并限制在所属屏幕可用区域内；内容随窗口缩放。窗口背景跟随应用窗口调色板。

弹窗 VM 的 tryClose(result)、Escape 和标题栏关闭都经私有 WindowConductor 调用 canClose。原关闭事件立即拒绝，排队检查许可；许可通过后恢复关闭，跳过本桥接的重复询问，原有 QML onClosing 仍可拒绝。

等待及提交期间重复关闭不重新检查、不覆盖首次结果。守卫拒绝、抛异常或 QML 拒绝时保留窗口和 VM、丢弃本次结果，Future 保持未完成；以后可重新请求关闭。守卫异常在 Qt 边界记录。守卫按主线程回调一次的现有契约执行，不增加通用请求队列或代次协调。

直接完成 deactivate(true) 会反向请求关闭窗口并跳过守卫；普通停用不关闭窗口。已尝试的关闭生命周期不自动重试，已完成的关闭不再执行一次。其他关闭处理仍可拒绝窗口关闭；直接提交的 VM 状态不会回滚。

## 所有权与清理顺序

showDialogAsync 按值消费 unique_ptr，拒绝时同样回收候选。只接纳无 QObject 父对象且与服务同线程的 QObject；实现 IChild 时要求无逻辑 Parent，实现 IActivate 时要求未激活；请求者须非空且同线程。错误线程候选通过 deleteLater 在所属线程回收，调用方负责其事件循环和寿命。

接纳后由 WindowManager 的 QObject 父树持有 VM，设置 CppOwnership，逻辑 Parent 保持为空。先装配内部窗口与业务 View，设置所属关系、尺寸、关闭桥接并创建隐藏的原生资源，再调用可选 IActivate，最后显示；Screen 首次初始化由 activate 完成。窗口同样由服务持有，transientParent 仅表达所属关系。确认 VM 通过 QPointer 借用服务，避免 shared_ptr 环。

实际关闭成功后先断开桥接并释放窗口父树，同步卸载、销毁 View/Loader；再执行一次 VM 关闭生命周期、安排 VM deleteLater，最后完成 Future。整个清理过程保持 busy。请求者销毁、解除关联、窗口/引擎失效及服务析构直接清理，不等待许可。VM 意外销毁时先完成其销毁通知，再结束窗口与 Future，不调用失效对象生命周期。外部直接销毁窗口时，等待窗口及子 View 析构完成后再执行 VM 生命周期。迟到许可通过桥接 QPointer 检查，不访问已销毁对象。

## 输入、焦点与业务接入

独立窗口以 ApplicationModal 限制应用其他窗口的用户输入；事件循环和程序调用继续执行。外部点击不关闭；Tab/Shift+Tab 在弹窗窗口内遍历，确认框默认聚焦取消。Escape 提交空结果，Return、Enter 和 Space 保持焦点控件的正常行为。

打开前保存所属窗口焦点，显示后激活弹窗并聚焦业务 View。正常关闭后排队激活所属窗口、恢复有效且可见启用的原焦点，否则使用后备焦点。已有新弹窗、强制清理、所属窗口不可见或销毁时跳过恢复。

ConfirmationRequest 保存文案，ConfirmActionViewModel 仅接收 ConfirmationRequest，通过 tryClose(true/false) 提交接受或取消，不依赖窗口服务。确认视图经 ViewLocator 的统一规则在框架搜索根内定位；应用可在匹配的搜索根下部署 `views/ConfirmActionView.qml`，也可在冻结前显式登记覆盖。选定覆盖后加载或注入失败不回退。Home 重置和 Detail 离开确认保留 Future 业务续接，只有 true 授权业务操作。页面以 QPointer 借用自己的确认框，停用时调用 tryClose(std::nullopt) 请求关闭；关闭检查排队执行，守卫或 QML 可拒绝。请求者销毁时由窗口服务自动强制清理，不提供公开的按请求者取消接口。

## 迁移与 CM 对齐边界

公开 closeDialog 已移除，调用方改用 vm->tryClose(result)；ConfirmActionViewModel 构造时不再传入窗口服务。相关消费工程需同步修改并重新编译。自定义实现 IWindowManager 的类须支持 showWindow(viewModel, options, context)、windowFor，以及 showDialogAsync(viewModel, requester, options, context)；覆盖方法的测试替身也须同步签名。原有 showWindow(vm)、showWindow(vm, options)、showDialogAsync(vm, requester) 和 showDialogAsync(vm, requester, options) 均继续有效，不需要插入 std::nullopt。直接在末尾追加可选 context，不添加兼容重载；自定义实现及覆写仍需同步新签名，接口实现与消费工程须重新编译。普通窗口传入 typed QVariant 并借用 VM，不能复用 showDialogAsync 的 unique_ptr 接管约定。

旧 DialogHost、DialogHostState 及其 manager/available/requestId/failed/dismiss/released 手动宿主协议已移除。应用应删除旧 QML 宿主声明，由 Bootstrapper 委托 showWindow 创建窗口，或在独立用法中直接调用 showWindow。原公开 attachToWindow / detachFromWindow 已收为 private，外部窗口手动接入不再支持。内部 DialogWindow 为私有 C++ 类型，不注册为公开 QML 类型；旧 DialogWindow.qml 资源已移除，测试和业务不得依赖其内部属性作为 SDK。

与 CM 3.2.0 对齐：独立窗口包装普通 View；窗口与 VM 统一守卫及双向关闭桥接。保留的适配差异是 QFuture 异步结果、Qt 对象所有权与延迟回收。单服务只允许一个弹窗；弹窗守卫中再次打开弹窗仍按忙状态失败，不支持嵌套模态窗口。系统强制退出与资源清理不等待交互许可。

根窗口定位、弹窗预检查和内部 ViewHost 均经 [ViewLocator](ViewLocator.md) 查询。窗口服务与根工厂均允许 QObject 模型。选定资源的加载、根类型或注入错误不会触发其他候选尝试。公开签名与迁移说明见 [接口化迁移](接口化迁移.md)。

## context 视图选择

context 是独立的 `const std::optional<QString>&` 参数，默认 `std::nullopt`：showWindow 的第三个参数、showDialogAsync 的第四个参数，均位于 options 之后。根窗口通过 `displayRootViewFor<T>(options, context)` 传递。WindowOptions 和 DialogOptions 仅保留展示配置，不保存 context。普通窗口将该参数传给 ViewLocator，弹窗预检与实际 ViewHost 装配使用同一个 context；指定 context 未匹配时窗口返回 false，弹窗 Future 以异常完成，不回退默认 View。参数职责与定位行为参考 CM 3.2.0；Qt 保留弹窗必填 requester 第二位，并将 context 追加到末尾以兼容旧业务调用，参数顺序不照搬 CM。只使用原有参数的业务调用无需修改；覆写签名仍须迁移，并重新编译消费工程。

context 只控制 View 选择，不放宽同一 VM 只能关联一个受管窗口的约束，不增加实例缓存或改变关闭守卫和所有权。完整用法见 [Multi-View](Multi-View.md)。
