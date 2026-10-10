# CM 3.2.0 关闭守卫与单项 Conductor 精简验收记录

2026-10-10，基于当前 HEAD `d3fea4b` 及此前未提交的 5B 实现继续精简。本轮已完成 macOS arm64 本机验收；麒麟和外部消费工程待验证。原 [5B 验收记录](5B验收记录.md) 及更早历史记录保持原样，本记录描述新的当前契约。未暂存、提交或推送。

## 交付与行为变更

- 删除 Conductor 的 Request、请求表、编号、生命周期取消监听、选择/成员代次及同步结果暂存。关闭链路为“调用策略 → 许可回调 → 同步执行”，不提供重叠请求、重复回调和旧许可失效保护。
- ICloseStrategy.execute 只接受成员列表和许可回调，删除 QObject context；默认策略只保留遍历、累计结果和同步/延后推进状态。逐项询问，拒绝后继续检查，全体同意才允许关闭，不支持部分关闭。
- 守卫、策略与同步生命周期异常直接传播，不转换为拒绝，不撤销已经提交的状态；延后执行异常由异步调用方处理。void 入口、IConductor /2.0 和 Screen 生命周期完成通知继续保留。
- 单项仅保存当前项及其销毁通知。canClose、getChildren 和父级生命周期只处理当前项；普通停用后的旧对象保留 QObject 与逻辑 Parent，不登记为受管成员。业务可重新激活有效、未关闭的旧对象；对非当前旧对象的关闭请求无操作，父级关闭不补发其关闭生命周期，QObject 父树兜底回收。
- 删除 PendingItem，候选由回调捕获的标准 RAII 所有权容器持有。基础校验失败不移动调用方所有权；同意后接管，拒绝或回调释放时回收。QPointer 仅检查管理者和借用目标存活，不实现请求协调。
- Detail 删除守卫编号与回调表。窗口失败先转换为 false，再交付许可；延后关闭执行异常在 Qt Future 边界记录。停用或销毁仍取消自己的弹窗。Shell 继续在许可通过后补 Home，取消不补建，工厂失败保留 Detail。
- 5A WindowManager 的内部请求机制与 Home 重置保持原样。Home VM 常驻、共享计数、View 每次新建及先卸载 View 后 deleteLater VM 的顺序继续保持。

当前接口与使用限制见 [关闭守卫](关闭守卫.md)、[Conductor](Conductor.md)。自定义关闭策略须迁移 execute 签名；消费工程须更新单项管理语义与异常预期。

## 环境与命令

macOS arm64、Qt 6.8.3、C++17，使用现有 macos-local 预设，独立构建目录。Qt 工具在沙箱外运行，避免已知 neon 特征检测限制；未修改本机预设。

```sh
cmake --preset macos-local -B /private/tmp/cmqt-cm-simplify-build
cmake --build /private/tmp/cmqt-cm-simplify-build -j 4
ctest --test-dir /private/tmp/cmqt-cm-simplify-build --output-on-failure -j 4
cmake --build /private/tmp/cmqt-cm-simplify-build --target all_qmllint -j 4
cmake --preset macos-local -B /private/tmp/cmqt-cm-simplify-framework-only   -DCALIBURN_BUILD_EXAMPLE=OFF -DCALIBURN_BUILD_TESTS=OFF
cmake --build /private/tmp/cmqt-cm-simplify-framework-only --target all all_qmllint -j 4
QT_QPA_PLATFORM=cocoa QT_QUICK_BACKEND=software /private/tmp/cmqt-cm-simplify-build/tests/CaliburnDialogTests
QT_QPA_PLATFORM=cocoa QT_QUICK_BACKEND=software /private/tmp/cmqt-cm-simplify-build/tests/CaliburnQmlTests
```

## 自动检查结果

最终 **35/35 个 CTest 入口通过**。Qt Test 数量包含 init/cleanup 和数据行，不代表断言数量。

| 检查 | 实际结果 |
| --- | --- |
| 全量构建与 all_qmllint | 通过，框架、示例与测试均完成构建和 QML 静态检查。 |
| 仅框架构建与 all_qmllint | 关闭示例及测试后通过。 |
| guardclose | 18 条通过；立即/延后接受或拒绝、拒绝后继续询问、已销毁快照项、单项/集合普通停用差异、嵌套当前项检查、集合全成员检查、候选拒绝/异常/回调释放回收、管理者销毁后回调安全、异常直接传播与不回滚。 |
| parent_protocol | 21 条通过；单项仅处理当前项、旧对象重新激活、非当前关闭无操作、父级只关闭当前项、旧对象父树回收，以及接口/Parent 通知顺序。 |
| composition | 26 条通过；确认结果、忙和失败、非活动集合 Detail 显式关闭、取消不补 Home、同步与延后工厂失败保留 Detail、重试及销毁取消。 |
| 既有回归 | conductor 28、collection 20、core 72、counterservice 56、windowmanager 17、QML 32、dialog 15 条通过；Bootstrapper 和框架弹窗独立入口全部通过。 |
| Cocoa | DialogTests 15 条、QmlTests 32 条通过，无失败或跳过。 |

首轮组合测试误将立即完成 Future 后的工厂异常预期为 Qt 边界处理；默认策略在同步遍历结束后执行续接，该异常应直接传播。校正测试预期并新增延后工厂失败场景后，组合测试和最终全量 CTest 均通过，无须修改框架异常路径。并发、重复回调、代次失效、强制立即回收测试已按新契约移除。

故意非法的 SyntaxError.qml 测试夹具扫描诊断为预期输出。构建和测试日志保存在本机 `/private/tmp/cmqt-cm-simplify-*.log`；逐项 Qt Test 结果位于 `/private/tmp/cmqt-cm-simplify-build/Testing/Temporary/LastTest.log`，临时日志不纳入交付。

## 实际窗口交互

使用最终构建 `/private/tmp/cmqt-cm-simplify-build/bin/CaliburnExampleApp.app` 验证：

- 初始 0，加 2 后显示 2；文本框输入 2 只修改文本。进入 Detail 后共享计数为 2，Home 已初始化但未活动。
- 返回确认文案正确，默认焦点为取消。弹窗数字键 2 不改变计数，Escape 保留 Detail；再次打开后默认 Return 取消，Detail 仍活动。
- 再次打开，Tab 切到确认、Return 接受，Home 恢复活动，Detail 无页面，计数仍为 2，Home 新 View 的文本框为空。
- 返回后数字键 2 增加到 4；文本框输入 2 不增加计数。重置确认接受后归零，焦点恢复到原文本框，文本仍为 2。
- 正常关闭窗口并退出应用。

## 使用边界

应用保证主线程、一次回调和模态期间参与状态稳定；不支持重叠关闭、重复回调、等待期间修改成员/生命周期或替换策略。等待期间禁止外部删除候选，已关闭对象不得复用；仍被外部回调持有的候选不承诺在管理者销毁时立即回收。

本轮没有加入主窗口关闭拦截、根 Shell 退出衔接、View 缓存或第六批功能。根 tryClose 仍无操作，Bootstrapper 清理直接执行生命周期。麒麟和外部消费工程待验证。
