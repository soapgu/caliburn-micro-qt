# 后续版本 ToDoList

本文件记录未实施的后续事项，不作为已支持接口或验收通过证据。

## 集合导航的视图保留机制（待设计、待实施）

4C 已保留集合中的 VM，但 ViewHost 在切换时销毁旧 View、返回时新建。后续考虑按 VM 身份复用仍在集合中的 View，使文本输入等界面状态随 View 保留。

职责边界：Conductor 管 VM，宿主管 View；成员关闭、VM 失效及宿主销毁时清理对应 View，保证 View 不超过借用 VM 的寿命。保持普通单项宿主默认卸载行为；缓存接口、启用方式、容量边界、焦点恢复及跨宿主限制另行确定。

参考 WPF Caliburn.Micro 的 [ViewAware](https://github.com/Caliburn-Micro/Caliburn.Micro/blob/master/src/Caliburn.Micro.Core/ViewAware.cs) 与 [ViewLocator](https://github.com/Caliburn-Micro/Caliburn.Micro/blob/master/src/Caliburn.Micro.Platform/ViewLocator.cs)：VM 按 context 弱引用关联 View，定位时优先复用仍可用的 View，否则创建。Qt 方案需明确对象所有权，不能直接照搬 .NET GC 行为；复用 View 与恢复键盘焦点分别设计。

后续验收须覆盖返回时 View 身份及文本保留、关闭清理、VM 意外销毁、宿主销毁、缓存隔离、重复往返及焦点行为。本轮不新增缓存实现或占位类型，当前行为见 [4C 验收记录](4C验收记录.md)。

## Conductor 关闭守卫（待设计、待实施）

为单项和集合型 Conductor 规划关闭许可检查，覆盖单项切换、显式关闭成员及父对象关闭。关闭被拒绝时保留相关对象、选择和页面，不提前移除成员或安排删除。普通停用和集合内切换不触发关闭守卫；当前同步生命周期契约保持不变。

同步或异步接口、集合关闭策略、取消与重复请求处理，以及 Bootstrapper 窗口退出的衔接留待独立设计。后续验收须覆盖接受、拒绝、部分成员拒绝、请求期间对象失效及单次回收等场景。本项仅记录后续目标，不新增实现或占位类型。

## Action 动作绑定（待设计、待实施）

将 Action／ActionMessage 风格的动作绑定列为后续能力，考虑目标解析、事件绑定、参数传递及可用状态联动。具体接口、方法匹配规则、错误诊断和绑定寿命留待独立设计。

当前继续使用手写 QML 属性绑定和事件处理器，不提前改变示例。Action 动作绑定与第五批 ConfirmActionViewModel 确认弹窗是不同能力，不绑定到第五批交付。本项尚未实现，不作为已支持接口或验收通过证据。
