# 后续版本 ToDoList

本文件记录尚未纳入当前批次的后续事项，不作为已支持接口或验收通过证据。Conductor 关闭守卫已在第五批 5B 实现并完成本机验收，见 [5B 记录](5B验收记录.md)，不再列为后续事项。

## 集合导航的视图保留机制（待设计、待实施）

4C 已保留集合中的 VM，但 ViewHost 在切换时销毁旧 View、返回时新建。后续考虑按 VM 身份复用仍在集合中的 View，使文本输入等界面状态随 View 保留。

职责边界：Conductor 管 VM，宿主管 View；成员关闭、VM 失效及宿主销毁时清理对应 View，保证 View 不超过借用 VM 的寿命。保持普通单项宿主默认卸载行为；缓存接口、启用方式、容量边界、焦点恢复及跨宿主限制另行确定。

参考 WPF Caliburn.Micro 的 [ViewAware](https://github.com/Caliburn-Micro/Caliburn.Micro/blob/master/src/Caliburn.Micro.Core/ViewAware.cs) 与 [ViewLocator](https://github.com/Caliburn-Micro/Caliburn.Micro/blob/master/src/Caliburn.Micro.Platform/ViewLocator.cs)：VM 按 context 弱引用关联 View，定位时优先复用仍可用的 View，否则创建。Qt 方案需明确对象所有权，不能直接照搬 .NET GC 行为；复用 View 与恢复键盘焦点分别设计。

后续验收须覆盖返回时 View 身份及文本保留、关闭清理、VM 意外销毁、宿主销毁、缓存隔离、重复往返及焦点行为。本轮不新增缓存实现或占位类型，当前行为见 [4C 验收记录](4C验收记录.md)。

## Action 动作绑定（待设计、待实施）

将 Action／ActionMessage 风格的动作绑定列为后续能力，考虑目标解析、事件绑定、参数传递及可用状态联动。具体接口、方法匹配规则、错误诊断和绑定寿命留待独立设计。

当前继续使用手写 QML 属性绑定和事件处理器，不提前改变示例。Action 动作绑定与 5A 已实现的 ConfirmActionViewModel 确认弹窗是不同能力，不绑定到 5A/5B 交付。本项尚未实现，不作为已支持接口或验收通过证据。

## 根窗口关闭请求与 tryClose 衔接（已实现）

根窗口与根 VM 通过 WindowManager::showWindow 创建的私有 WindowConductor 双向关联。窗口关闭先检查根 canClose，根 tryClose 请求关联窗口关闭；根直接完成关闭生命周期后反向关闭窗口。WindowManager 执行正常关窗后的根停用，Bootstrapper 协调根兜底清理和 OnExit，完整契约见 [Bootstrapper](Bootstrapper.md)、[关闭守卫](关闭守卫.md)，结果见 [根窗口验收](根窗口关闭守卫与生命周期验收记录.md)。

后续仍不包含多窗口退出协调、系统强制终止交互拦截及 View 缓存；麒麟与外部消费工程待验证。

独立应用级模态窗口及弹窗关闭守卫已实现，保留 Future 异步返回和单弹窗限制。嵌套弹窗、同步 ShowDialog 返回方式仍不在当前接口内，见 [窗口服务](WindowManager.md) 与 [独立模态窗口验收](独立模态窗口验收记录.md)。
