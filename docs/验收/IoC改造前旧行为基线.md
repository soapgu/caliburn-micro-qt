# IoC 改造前旧行为基线

验收日期：2026-11-12。本记录对应 [IoC 改造方案](../IoC改造方案.md)步骤 1，不代表实例提供器或窗口生命周期新契约已经实现。

## 环境与基线

| 项目 | 实际值 |
| --- | --- |
| 源码基线 | `6d9fad229b8765b648b6b2cf93f47f58d595f029` |
| 操作系统 | macOS 27.0.1，构建号 26A434 |
| 主机／测试程序架构 | arm64；测试程序为 Mach-O 64-bit arm64 |
| Qt | 6.8.3，`/Users/guhui/Qt/6.8.3/macos` |
| 工具 | CMake 3.22.1、Ninja 1.12.1、Apple clang 21.0.0 |
| 构建目录 | 复用 `build/debug`，Debug，示例与测试均开启 |
| 缓存源码目录 | `/Users/guhui/Githubs/caliburn-micro-qt`，与当前仓库一致 |
| GUI 测试 | CTest 既有 `offscreen/software` 环境；涉及控件的测试使用 Basic 样式 |

开始时工作区干净。首次构建显示 `ninja: no work to do.`，随后运行原有完整 CTest；补充测试后的结果独立记录。生产源码、公开 API、下游应用和依赖配置未修改。

## 命令与结果

以下命令均在仓库根目录执行。日志存放在忽略的构建目录 `build/debug/ioc-baseline/`，不纳入版本控制。

```sh
# 补测试前
cmake --build --preset debug
ctest --preset debug --output-log build/debug/ioc-baseline/ctest-before.log

# 补测试后
cmake --build --preset debug
ctest --test-dir build/debug -R '^bootstrapper_' --output-on-failure \
  --output-log build/debug/ioc-baseline/ctest-targeted.log
ctest --preset debug --output-log build/debug/ioc-baseline/ctest-after.log
cmake --build build/debug --target all_qmllint

git diff --check
```

| 检查 | 结果 |
| --- | --- |
| 修改前构建 | 通过，无需重新编译 |
| 修改前完整 CTest | 109/109 通过，35.22 秒 |
| 修改后构建 | 通过，完成 Bootstrapper 测试重新编译和链接 |
| 修改后 Bootstrapper 定向 CTest | 30/30 通过，6.36 秒 |
| 修改后完整 CTest | 110/110 通过，32.06 秒 |
| all_qmllint | 通过；框架、minimal、multiscreen、multiview 四个模块检查成功 |
| 差异空白检查 | 通过 |

构建输出分别见 `build-before.log`、`build-after.log`，测试控制台输出见 `ctest-before-console.log`、`ctest-targeted-console.log`、`ctest-after-console.log`，QML 检查输出见 `qmllint.log`。CTest 的详细逐场景诊断还可在 `build/debug/Testing/Temporary/LastTest.log` 查看，该文件会被后续运行覆盖。

## 旧行为与验证映射

测试源码主要位于 `tests/cpp/tst_bootstrapper.cpp`、`tst_windowmanager.cpp`、`tst_multiscreen.cpp`，场景由 `tests/CMakeLists.txt` 登记。下表的 `bootstrapper_*` 与 `rootwindow_*` 是独立 CTest 项；WindowManager 方法由 `windowmanager` 测试项执行。

| 旧行为 | 证据与具体断言 |
| --- | --- |
| 默认窗口服务在 configure 前创建 | 静态核对 `BootstrapperBase::initialize()`：先创建 WindowManager、连接清理失败和关闭通知、设置 CppOwnership，再调用 configure。没有公开入口可直接观察创建瞬间；本项属于源码证据，不宣称已有运行时创建时序断言 |
| 根工厂使用框架实际窗口服务 | `bootstrapper_success` 等成功路径断言传入服务的 `windowFor(shell)` 等于实际根 View，且根窗口内部服务属性指向该服务；`injectedService` 验证业务通过该服务打开弹窗 |
| 根工厂仅配置阶段可登记 | 所有 startupAndCleanup 数据行在 run 前和返回后均尝试登记并断言失败；`emptyFactory` 验证空工厂失败 |
| 同类型根工厂不可重复 | 新增 `bootstrapper_duplicateFactory`：第二次登记失败、run 返回 1、工厂未执行、未进入事件循环、onExit 仅一次 |
| 单根及单次显示限制 | `bootstrapper_duplicateRoot`：第二次显示失败、工厂仅执行一次、启动返回 1并清理；`windowmanager` 的 `normalWindowRejectsWithoutActivating` 验证第二主窗被拒绝且候选不激活 |
| QML 保留具体 VM 指针类型 | 成功启动数据行检查根 viewModel 的 metaType 为 ShellViewModel*，Home View 绑定实际 Home 对象 |
| View 创建绑定先于激活显示 | `bootstrapper_activationDialog` 在根 onActivate 中断言 View 已绑定 this、服务已关联、窗口尚未显示，并能打开弹窗；`windowmanager/activationSeesBoundHiddenViewAndCanOpenDialog` 还检查原生窗口资源已创建，QML 创建时模型尚未初始化和激活 |
| 正常关闭和守卫取消 | `rootwindow_home/accept/cancel/escape/rootTryClose/modelClose` 验证关闭次数、确认框与取消后活动页保留；WindowManager 的 `normalWindowWithoutBootstrapper` 覆盖独立窗口守卫 |
| 辅助窗口关闭不退出主窗 | `multiscreen_windowsAndExit_auxiliary` 验证辅助窗销毁后主窗仍可见且活动，再由主窗退出；WindowManager 的 `auxiliaryCloseKeepsPrimaryAndOtherWindows` 验证其他普通窗口不受影响 |
| onExit 至多一次 | 所有 startupAndCleanup 数据行检查 exitCalls 为 1，重复 run 返回 1，析构后计数仍为 1；`exitException` 验证异常也不重复调用 |
| onExit 时根和普通 View 尚存 | 成功启动数据行检查根存活且不活动，onExit 内验证服务仍关联记录的根窗口；事件顺序严格为 close、exit、view.destroy、shell.destroy |
| View 先于借用 VM 释放 | startupAndCleanup 比较根及 Home 的 View／VM 销毁顺序；rootwindow 场景验证 Detail View 先于 Detail VM 销毁 |
| 外部保留服务不延长根 View 寿命 | `bootstrapper_retainedService`：run 后服务仍存活但全部窗口已释放、无当前弹窗且不忙，根与页面已销毁；释放最后服务引用后 QPointer 失效 |
| 配置／创建／显示失败 | `configureFailure/missingFactory/factoryException/nullRoot/missingMapping/remoteView/missingResource/nonWindow/noRoot/ignoredLoadFailure/hostFailure` 检查返回 1、事件循环是否进入、工厂和退出调用次数及对象释放 |
| 生命周期异常及返回码 | `exitException/closeException/windowCloseException` 返回 1并继续清理；`externalCloseException` 固化已有外部捕获异常后的返回 0行为；`exitCode` 保留退出码 7 |
| 强制退出与在途弹窗清理 | `rootwindow_forcedExit/forcedExitDialog` 检查根关闭一次；在 onExit 前弹窗已释放、Future 已完成、busy 已清空 |
| 多窗口失败回滚 | `multiscreen_startupFailureRollback` 检查返回 1、窗口清空及 quitOnLastWindowClosed 恢复；destroy 场景验证意外销毁返回 1 |
| 重复 run 被拒绝 | startupAndCleanup 在首次 run 后再次调用并断言 1，退出钩子不再执行 |

默认服务的提前创建顺序已记录为后续改造必须核对的源码基线；本次没有为观测该私有细节引入生产测试钩子或修改访问控制。

## 本次补充的测试

- 新增 `duplicateFactory` 数据行及对应独立 CTest 登记，覆盖同类型根工厂重复登记。
- 为既有 startupAndCleanup 数据行增加 run 前、run 后登记拒绝断言，复用原夹具，不新增注册表重置入口。
- 为既有成功路径增加根窗口与注入服务的身份断言，并在 onExit 验证根 View 仍由同一服务关联。
- 强化既有 activationDialog：在根激活钩子中直接断言 View 已完成模型绑定和窗口关联，且尚未显示。

现有测试已经直接覆盖的关闭守卫、释放顺序、保留服务及辅助窗口行为未另写重复测试。涉及进程级映射与冻结的 Bootstrapper 场景仍由 CTest 分进程执行。

## 诊断、边界与结论

负向测试会主动输出 QCRITICAL／QWARN，例如阶段外登记、重复根工厂、重复 run、缺少映射、QML 加载失败及测试主动抛出的启动／关闭／退出异常。这些诊断与对应返回码断言共同组成预期行为，不按日志级别直接判断失败。故意非法的 QML 夹具不参与正常模块 qmllint。

本轮未发现需要修改生产实现的缺陷，未进入 IoC 新接口实现。本机自动化旧行为基线通过：修改前 109 项、修改后 110 项全部通过，构建与 QML 检查通过；后续框架改造应保持本页记录的旧行为。

未执行真实桌面人工交互、现场四屏、麒麟平台和红厅应用 F2 兼容重建。offscreen 多屏测试仅验证布局决策及主辅窗口程序行为，不能代替实际多屏设备验收。本次不声明 ABI 兼容，不提交或推送变更。
