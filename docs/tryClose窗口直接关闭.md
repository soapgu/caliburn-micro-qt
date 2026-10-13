# tryClose：通过关联窗口直接关闭

本轮删除 ScreenViewModel.closeRequested 信号及全部订阅，保留 `void tryClose(DialogResult = std::nullopt)` 调用形式。消费工程需删除旧信号连接并重新编译，IWindowManager、Conductor 和生命周期能力接口签名保持不变。

## 关闭路径

1. 逻辑 Parent 实现 IConductor 时调用 deactivateItem(this, true)，忽略结果参数。
2. 否则通过内部 QPointer 获取当前受管窗口。非 Conductor 的逻辑 Parent 不阻断窗口回退，QObject 父对象不代替逻辑 Parent。
3. 弹窗先向内部窗口的 dialogResult 属性写 bool 或空值，再通过 Qt 元对象直接调用窗口 close()；普通窗口忽略结果。
4. 没有有效窗口时记录诊断并返回，不停用模型，不缓存提前关闭请求。

CM 3.2.0 的 Screen.TryClose 通过 PlatformProvider 查找 Parent Conductor 或关联 View，再设置 DialogResult/调用 Close。本轮沿用其调用方向，保留 Qt 的异步 Future、排队守卫及显式所有权差异；没有引入 IViewAware、ViewAware 或新的公开关闭接口。

## 最小窗口弱关联

私有 WindowViewBinding 由 WindowConductor 持有，只建立 Screen 到当前窗口的弱引用。桥接解除或销毁时清除关联，窗口销毁时 QPointer 失效，模型销毁后绑定清理不再访问模型。普通 QObject 模型不要求此能力。

同一个 Screen 只能关联一个受管窗口；窗口服务在创建、激活前拒绝重复展示。窗口销毁后可以重新绑定，旧绑定的延后清理通过绑定身份检查保证不清除新关联。此机制不拥有窗口或模型，也不关联页面 View，不提供缓存、context 或公开的手动绑定 API。

内部 dialogResult 属性只表达候选结果。WindowConductor 在首次关闭事件时固定结果，等待或提交期间后续 tryClose 不改写本次结果；守卫拒绝、异常或 QML 拒绝后清空固定结果及窗口属性，允许重新请求。成功关闭后将固定结果交给 WindowManager 完成 Future。Escape、标题栏、直接关闭生命周期及强制清理返回空结果。

## 创建与生命周期顺序

普通窗口及弹窗统一为：创建并绑定 View → 配置窗口 → 建立弱关联和关闭桥接 → 创建隐藏原生资源 → 可选激活 → 显示。

普通窗口由服务注入初始 visible=false，避免 QML 的初始 visible:true 提前显示；业务不应在初始装配回调中自行显示窗口。Qt 6.8.3 的 QWindow::close 在没有原生资源时直接返回，因此激活前调用 create()，不显示窗口，也能让激活钩子的 tryClose 产生关闭事件。关闭守卫仍在调用栈退出后排队执行，方法返回不表示关闭完成。

根激活前已建立所属窗口和引擎，允许在激活钩子中使用 IWindowManager 打开确认弹窗。构造或 QML 初始装配阶段尚未保证窗口关联，这些阶段的 tryClose 不排队保存。加载、根类型或注入失败不激活模型；激活异常先解除关联并销毁 View，再对已进入展示生命周期的对象执行一次关闭收尾。

普通窗口继续借用模型，弹窗继续消费 unique_ptr；QPointer 不延长对象寿命。保留 IDeactivate 通知、防循环和异常后不重试。根退出仍先关闭模型再 onExit，之后销毁普通 View/引擎，最后删除根模型；弹窗仍先销毁 View，再执行关闭并安排模型回收。

## 验证与迁移

原信号次数测试改为检查窗口关闭、守卫、结果及实际生命周期。无 GUI 的业务测试通过 QObject 关闭端点和同一私有绑定机制承接 tryClose，不需要 QQuickWindow 或 GUI 应用。

关键验证包括窗口关联及解绑、重复绑定、旧清理隔离、Parent 优先与回退、激活期间关闭、首次结果保留、原生关闭优先、拒绝和异常后重试、加载失败不激活、激活失败回收、根激活期间打开弹窗，以及现有 QObject/Screen、导航、退出与所有权回归。结果见 [本轮验收](验收/tryClose窗口直接关闭验收记录.md)。
