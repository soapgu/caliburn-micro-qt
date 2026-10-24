# 后续版本 ToDoList

本文件仅保留尚未完成的后续事项；已支持接口和验收证据以对应专题文档、验收记录为准。

## 视图定位约定的后续扩展（待设计、待实施）

视图自动定位第一阶段纳入 [第七批：视图约定定位与自动装配](迭代实现计划.md#第七批视图约定定位与自动装配)，实现搜索根配置、`ViewModel → View`、同命名空间定位、`ViewModels → Views` 映射、显式覆盖，以及根窗口、页面、弹窗的统一接入。以下能力不属于第一阶段，待后续独立设计和实施：

- `Page` 后缀约定：参考 CM 3.2.0 的后缀配置，支持 `HomePageViewModel → HomePage` 等命名，明确与 `View` 后缀规则的候选顺序。
- 自定义转换规则：支持应用扩展名称与命名空间转换，明确规则配置入口、匹配顺序及与默认约定的关系。

context 多视图定位已独立实现，接口与对齐边界见 [Multi-View](../Multi-View.md)，验证见 [验收记录](../验收/Multi-View验收记录.md)。它决定“选择哪个 View”；下文的 View 实例保留决定“是否复用已有实例”，两者独立，不因支持 context 定位就启用实例缓存。上述 Page 后缀和自定义转换规则仍未实施，不提前增加占位类型或验收通过记录。

## 普通窗口自动包装 Item View（待设计、待实施）

当前 `WindowManager::showWindow` 要求 View 根对象为 `QQuickWindow`（QML 的 `Window` / `ApplicationWindow`）。后续支持根为 `Item` 的普通 View：由框架创建顶层窗口并承载界面内容，使业务 QML 无需自行声明窗口外壳；根为窗口的 View 继续沿用现有直接展示方式。

设计时需明确包装窗口的默认尺寸、标题、窗口标志及配置入口，保证类型化 `viewModel` 注入、内容随窗口缩放、激活与关闭守卫、`tryClose` 窗口关联和资源释放顺序保持一致。包装窗口与 View 由窗口服务管理，普通窗口 VM 仍由调用方持有。

后续验收覆盖 Item View 自动包装、原生窗口 View 直接展示、加载或注入失败清理，以及关闭守卫、生命周期和 View 先于 VM 释放。本项独立于多窗口管理与多屏布局；尚未实施，不提前增加接口或占位类型。

## 多窗口弹窗归属泛化（待设计、待实施）

初版多窗口已实现一个主窗和多个独立辅助窗，弹窗统一属于主窗。后续再考虑将弹窗归属泛化到不同业务窗口；本项暂不展开设计。

## 集合导航的视图保留机制（待设计、待实施）

4C 已保留集合中的 VM，但 ViewHost 在切换时销毁旧 View、返回时新建。后续考虑按 VM 身份复用仍在集合中的 View，使文本输入等界面状态随 View 保留。

职责边界：Conductor 管 VM，宿主管 View；成员关闭、VM 失效及宿主销毁时清理对应 View，保证 View 不超过借用 VM 的寿命。保持普通单项宿主默认卸载行为；缓存接口、启用方式、容量边界、焦点恢复及跨宿主限制另行确定。

参考 WPF Caliburn.Micro 的 [ViewAware](https://github.com/Caliburn-Micro/Caliburn.Micro/blob/master/src/Caliburn.Micro.Core/ViewAware.cs) 与 [ViewLocator](https://github.com/Caliburn-Micro/Caliburn.Micro/blob/master/src/Caliburn.Micro.Platform/ViewLocator.cs)：VM 按 context 弱引用关联 View，定位时优先复用仍可用的 View，否则创建。Qt 方案需明确对象所有权，不能直接照搬 .NET GC 行为；复用 View 与恢复键盘焦点分别设计。

后续验收须覆盖返回时 View 身份及文本保留、关闭清理、VM 意外销毁、宿主销毁、缓存隔离、重复往返及焦点行为。本轮不新增缓存实现或占位类型，当前行为见 [4C 验收记录](../验收/4C验收记录.md)。

## Action 动作绑定（待设计、待实施）

将 Action／ActionMessage 风格的动作绑定列为后续能力，考虑目标解析、事件绑定、参数传递及可用状态联动。具体接口、方法匹配规则、错误诊断和绑定寿命留待独立设计。

当前继续使用手写 QML 属性绑定和事件处理器，不提前改变示例。Action 动作绑定与 5A 已实现的 ConfirmActionViewModel 确认弹窗是不同能力，不绑定到 5A/5B 交付。本项尚未实现，不作为已支持接口或验收通过证据。
