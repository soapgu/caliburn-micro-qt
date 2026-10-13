# tryClose 窗口直接关闭验收记录

2026-10-13，macOS arm64 / Qt 6.8.3。实现精简窗口弱关联和先创建绑定、再激活显示的顺序。本轮未暂存、提交或推送，历史验收记录保持原样。

## 实现范围

- 删除 ScreenViewModel.closeRequested 信号及全部订阅；tryClose 优先委托逻辑 Parent IConductor，否则直接调用弱关联窗口 close。非 Conductor 的逻辑 Parent 也回退到窗口，无关联时诊断并返回。
- 私有 WindowViewBinding 不拥有模型或窗口；桥接解除、窗口或模型失效时安全清理。重复 Screen 关联在创建/激活前拒绝，旧绑定清理不影响新关联。
- 内部弹窗 dialogResult 承载候选 bool/空结果，首次关闭事件固定本次结果；等待期间不覆盖，守卫拒绝、异常或 QML 拒绝后清空，成功关闭交付 Future。移除旧的 closePending 信号缓存和不再使用的 closeRejected 通知。
- 窗口先创建并注入模型、关联所属窗口/引擎和关闭桥接，调用 create() 创建隐藏原生资源，再激活并显示。普通窗口注入初始 visible=false，避免 QML 默认 visible:true 提前显示。
- 加载失败不激活；激活异常解除关联、释放 View，再执行一次生命周期收尾。保留 QObject 能力分派、所有权、防循环、关闭不重试及根 onExit 寿命。
- 无 GUI 测试通过 QObject 关闭端点和相同私有绑定机制接入，保留即时结果、延后取消和请求者销毁测试。

公开信号删除和 Screen 类布局改变要求消费工程重新编译。IWindowManager、Conductor 和 IDeactivate 等能力接口签名不变；不增加完整 ViewAware、页面关联、context、缓存或新的公开关闭接口。当前契约见 [迁移说明](../tryClose窗口直接关闭.md)。

## 构建及自动检查

| 检查 | 结果 |
| --- | --- |
| macos-local 框架、示例及测试构建 | 通过，无编译警告 |
| 完整 CTest，offscreen/software | 83/83 通过，8.16 秒 |
| all_qmllint | 通过，无诊断 |
| Cocoa 窗口服务定向测试 | 24 passed，0 failed，约 5.7 秒 |
| Cocoa QObject 能力回归 | 31 passed，0 failed，约 10.3 秒 |
| Cocoa 根激活钩子打开弹窗 | 场景通过；含初始化/清理共 3 passed |
| git diff --check | 通过 |

QtTest 的 passed 数包含初始化与清理。构建和 Qt 运行在沙箱外执行，以避开本机 Qt 工具的已知 NEON 沙箱检测问题。

```sh
cmake --build --preset macos-local -j 6
ctest --test-dir build/debug --output-on-failure -j 4
cmake --build build/debug --target all_qmllint
QT_QPA_PLATFORM=cocoa QT_QUICK_BACKEND=software build/debug/tests/CaliburnWindowManagerTests activationSeesBoundHiddenViewAndCanOpenDialog ordinaryCloseDuringActivation duplicateWindowBindingRejectedBeforeActivation normalWindowLoadFailure results closeDuringActivation resultDuringNativeCloseIsIgnored guard qmlVetoAndGuardException pageDeactivationRequestsConfirmationClose
QT_QPA_PLATFORM=cocoa QT_QUICK_BACKEND=software build/debug/tests/CaliburnObjectCapabilitiesTests
QT_QPA_PLATFORM=cocoa QT_QUICK_BACKEND=software build/debug/tests/CaliburnBootstrapperTests 'startupAndCleanup:activationDialog'
```

新增 windowviewbinding 入口覆盖无窗口不停用、bool/空结果写入、普通窗口忽略结果、解除绑定、重复绑定拒绝、窗口销毁后重绑、旧绑定清理隔离及模型销毁后清理。Parent 协议测试验证 Conductor 优先和非 Conductor Parent 回退。

窗口服务测试验证激活时 View 已创建、模型尚未在 View 创建时激活、窗口有原生资源但尚未显示；激活钩子能够打开弹窗和直接 tryClose。既有接受/取消/空结果、重复请求、原生关闭优先、守卫与 QML 拒绝后重试、页面停用关闭确认框、对象销毁、强制清理及激活异常全部通过。加载失败及 Bootstrapper 对应预期改为不初始化、不激活、不执行 Screen 关闭钩子。

日志位于 `/private/tmp/cmqt-tryclose-build.log`、`/private/tmp/cmqt-tryclose-ctest.log`、`/private/tmp/cmqt-tryclose-qmllint.log`、`/private/tmp/cmqt-tryclose-cocoa-windowmanager.log`、`/private/tmp/cmqt-tryclose-cocoa-objects.log`、`/private/tmp/cmqt-tryclose-cocoa-bootstrapper.log`；临时路径不作为长期交付文件。

## 原生示例交互

使用本次构建的 CaliburnExampleApp 完成以下桌面操作：

1. 加 2 后显示计数 2，打开重置弹窗，默认聚焦取消。
2. Escape 关闭弹窗，计数保持 2。
3. 再次打开并确认，tryClose(true) 直接关闭窗口，Future 交付接受结果，计数归零。
4. 进入详情，Home 停用、Detail 激活；返回确认选择取消，详情及活动状态保持。
5. 在详情关闭主窗口，出现离开确认；接受后应用退出，应用清单确认 isRunning=false。

本机验收未覆盖麒麟或外部消费工程。
