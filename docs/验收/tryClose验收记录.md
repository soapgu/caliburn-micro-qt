# tryClose 与 Detail 自关闭返回验收记录

2026-10-08，基于提交 `9f2d64a` 实施。受管页面 tryClose、Detail 内返回、自动检查、Cocoa 及实际窗口验收已完成，本增量本机验收通过；麒麟待验证。未暂存、提交或推送。历史 Parent、4C 等验收记录和第四批本机完成状态保持原样。

## 本轮交付

- Screen 新增普通 C++ bool tryClose()，每次通过逻辑 Parent 转换为 IConductor，委托 deactivateItem(this, true)；不向 QML 暴露，不缓存管理者、不使用 QObject 父对象兜底。
- 无逻辑 Parent、接口不匹配或管理者拒绝时返回 false；true 表示请求已处理，实际销毁可以延迟。C++ 异常继续传播，没有生命周期回滚。
- Detail 新增 Q_INVOKABLE bool goBack()，仅活动时调用 tryClose，在 QML 业务操作边界捕获并记录异常、返回 false，不引用 Shell 或 Home。
- 返回按钮移入 DetailView，objectName 为 goBack；Shell 顶部只保留查看详情，homeHost、Home 控件和生命周期文字标识保留。Detail 显式配置 Tab 导航。
- Shell 重写公共 deactivateItem(ViewModelBase*, bool)，仅关闭当前 Detail 时先 ensureHome，再限定调用集合元对象基类。Home 恢复失败时尚未关闭 Detail，可以修复工厂后重试。返回操作由 Detail.goBack 发起，经 Detail.tryClose 共用协议路径，C++ 异常契约保留。
- 既有单项及集合型算法文件、buildShell 装配和 CounterService 寿命保持不变。未实现根窗口关闭请求、关闭守卫、异步接口、Action 或 View 缓存。根 Shell.tryClose 返回 false，不关闭窗口。

## 环境与命令

macOS arm64、Qt 6.8.3、C++17，使用现有 macos-local 预设；项目位于 /Users/guhui/Githubs/caliburn-micro-qt。独立构建、测试和实际应用在沙箱外执行，没有修改本机预设。

```sh
cmake --preset macos-local -B /private/tmp/cmqt-tryclose-build
cmake --build /private/tmp/cmqt-tryclose-build -j 4
ctest --test-dir /private/tmp/cmqt-tryclose-build --output-on-failure
cmake --build /private/tmp/cmqt-tryclose-build --target all_qmllint
cmake --preset macos-local -B /private/tmp/cmqt-tryclose-framework-only \
  -DCALIBURN_BUILD_EXAMPLE=OFF -DCALIBURN_BUILD_TESTS=OFF
cmake --build /private/tmp/cmqt-tryclose-framework-only -j 4
cmake --build /private/tmp/cmqt-tryclose-framework-only --target all_qmllint
QT_QPA_PLATFORM=cocoa /private/tmp/cmqt-tryclose-build/tests/CaliburnQmlTests
/private/tmp/cmqt-tryclose-build/bin/CaliburnExampleApp.app/Contents/MacOS/CaliburnExampleApp
```

## 自动检查结果

最终全部 25 个 CTest 入口通过，没有新增测试目标。Qt Test 数量包含 init/cleanup，不代表断言数量。

| 检查 | 实际结果 |
| --- | --- |
| 独立配置及全量构建 | 通过；Screen、Shell 协议 override、Detail 元对象及 QML 资源编译成功。 |
| parent_protocol | 21 条通过；覆盖逻辑 Parent 委托、禁止 QObject 父对象兜底、接口不匹配、拒绝与异常、每次重新读取 Parent、单项当前及留存项关闭、嵌套集合关闭、重复请求和父树提前回收。原 Parent/通知顺序测试继续通过。 |
| collection | 20 条通过，回归原集合算法与生命周期。 |
| counterservice | 56 条通过，计数边界和通知保留。 |
| conductor | 28 条通过，原单项算法及生命周期回归。 |
| core | 67 条通过，原属性、业务操作、元对象断言回归。 |
| composition | 18 条通过，覆盖 Detail 返回、非活动拒绝、根请求返回 false、Home 缺失恢复、空返回/接管拒绝/标准及未知异常、C++ 异常传播、goBack 诊断和失败后重试。服务共享、隔离及单次回收回归。 |
| qml（offscreen） | 28 条通过；页面内按钮自关闭、重新获取控件、Home VM 和计数保留、文本重建、Tab/空格返回、View 先于 VM 销毁及真实 QML 点击失败后重试通过。根 tryClose 不关闭可见窗口。 |
| Bootstrapper | 原 18 个独立入口全部通过，包含实际示例进入 Detail 后退出。 |
| all_qmllint | 框架和示例用户模块通过。 |
| 仅框架构建及 qmllint | 通过；生成的 build.ninja 中没有 Boost DI 或 CaliburnExample 目标。 |
| Cocoa 原生 QML 测试 | 最终 28 passed、0 failed、0 skipped，退出码 0。 |

tryClose 不在 Screen/Detail 元对象可调用方法中，Detail.goBack 可通过元对象调用。失败恢复测试分别验证 C++ tryClose 保留异常、goBack 捕获诊断并返回 false，失败期间成员、选择、Detail Parent 和活动通知均无变化，随后成功恢复。

首轮 Cocoa 发现新 Detail 按钮无法按平台默认 Tab 策略获得焦点；沿用 Home 的显式 KeyNavigation 方式后，针对性 Cocoa 键盘返回测试通过。后续全量 Cocoa 暴露原 Home 键盘测试仅等待窗口显示、尚未等待活动状态的时序问题；增加 requestActivate 和 qWaitForWindowActive 后，全量 Cocoa 28 条通过。未放宽焦点断言或屏蔽失败。

故意非法 QML 夹具诊断、既有静态插件重复链接提示及 Cocoa 输入法日志仍可出现；这些输出不是通过证据，也没有通过警告抑制绕过接口问题。

## 返回接口清理回归

2026-10-08，确认应用已无 Shell.goHome/canGoHome 调用后，删除这两个接口，现有装配测试统一覆盖 Detail.tryClose/goBack。Shell 的公共关闭入口及 Home 缺失恢复保持不变；QML 保留旧顶部返回按钮不存在的断言。清理后增量构建、全部 25 个 CTest 入口及框架/示例 all_qmllint 通过。本次接口清理未修改 QML，未重复执行 Cocoa 和实际窗口操作；下述记录来自本轮此前的实际验收。

## 实际窗口验收

绑定 /private/tmp/cmqt-tryclose-build/bin/CaliburnExampleApp.app 完整路径，通过原生 UI 操作与截图验证本轮产物：

| 操作 | 观察结果 |
| --- | --- |
| 启动 Home | Shell/Home 初始化且活动，Detail 无页面；顶部只有查看详情，计数 0。 |
| 点击增加、加 2，在文本框输入 tryClose2 | 计数 3，文本数字没有触发页面计数。 |
| 进入 Detail | Home 停用但保留，Detail 活动，显示共享计数 3；返回按钮位于详情内容内。 |
| 点击 Detail 内返回首页 | Detail 无页面，Home 恢复活动；计数仍为 3，文本框重新为空。 |
| 返回后按数字键 2 | 页面焦点有效，计数变为 5，加数按钮禁用。 |
| 再进入 Detail，按 Tab | 返回首页按钮获得键盘焦点，显示共享计数 5。 |
| 按空格返回 | Detail 自关闭，Home 活动，计数仍为 5。 |
| 重置，在文本框按 2，再按 Tab | 计数 0、文本 2；Tab 到达增加按钮。 |
| 点击空白处再按 2 | 页面焦点恢复，计数变为 2。 |
| 第三次进入 Detail 后关闭窗口 | 详情显示共享计数 2；直接启动的进程退出码 0。关闭后未再读取可能自动启动应用的 UI 状态。 |

Home 缺失及工厂失败由自动测试注入，未将其写成实际窗口人工操作结果。

## 边界与后续

同步、主线程、转换非重入和钩子正常返回约定继续适用。Detail 捕获异常不提供部分生命周期转换回滚。已有模板 closeItem 保持原算法；示例返回前置恢复经公共 IConductor 虚接口完成。

根窗口请求、关闭守卫、Action 和 View 保留继续见 [后续 ToDoList](../计划/后续版本ToDoList.md)。当前接口见 [Screen](../ScreenViewModel.md#tryclose受管页面请求关闭自己)、[Conductor](../Conductor.md#screentryclose-与公共协议) 和 [应用装配](../IoC与应用装配.md#detail-自关闭返回)。历史 [Parent 验收](Parent体系验收记录.md) 与 [4C 验收](4C验收记录.md) 未修改。麒麟及外部消费工程本轮未验证。
