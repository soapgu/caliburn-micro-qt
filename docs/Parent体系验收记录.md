# Parent 体系与统一 Conductor 协议验收记录

2026-10-08，基于提交 `9fe2e61` 实施。源码、自动检查、Cocoa 和实际窗口验收完成，本增量本机验收通过；麒麟待验证。第四批本机完成状态和历史验收记录保持原样。本轮未暂存、提交或推送。

## 本轮交付

- 新增纯 C++ 接口 IChild、IParent、IConductor，以及抽象元对象基类 ConductorBase。接口支持 qobject_cast，两个管理算法独立实现统一协议。
- Screen 实现 IChild，提供只读 QML parentViewModel，逻辑 Parent 使用 QPointer，框架维护，QObject 父树继续负责所有权。普通 VM 默认不实现 IChild，自定义实现也能接入。
- 单项 deactivateItem(item, false) 清空选择并留存 VM，允许原指针恢复、关闭非当前留存项及多个对象留存。getChildren 只枚举当前项，已关闭对象不能恢复；父关闭清理全部当前和留存项。
- 集合型 getChildren 返回全成员快照，普通停用保留选择，既有相邻选择、父关闭和意外销毁策略保留。
- 关闭前清空逻辑 Parent，成员和选择通知看到更新后关系；新增 activationProcessed，统一成功、拒绝、重复选择及自动相邻选择的结果发布。
- Shell 装配、共享服务寿命及 Home/Detail 导航保持原行为。View 每次新建；tryClose、根窗口请求、关闭守卫、异步接口、Action 和 View 缓存仍未实现，见 [后续 ToDoList](后续版本ToDoList.md)。

## 环境与命令

macOS arm64、Qt 6.8.3、C++17，使用现有 macos-local 预设。项目位于 /Users/guhui/Githubs/caliburn-micro-qt；独立构建和测试在沙箱外执行，没有修改本机预设。

```sh
cmake --preset macos-local -B /private/tmp/cmqt-parent-build
cmake --build /private/tmp/cmqt-parent-build -j 4
ctest --test-dir /private/tmp/cmqt-parent-build --output-on-failure
cmake --build /private/tmp/cmqt-parent-build --target all_qmllint
cmake --preset macos-local -B /private/tmp/cmqt-parent-framework-only \
  -DCALIBURN_BUILD_EXAMPLE=OFF -DCALIBURN_BUILD_TESTS=OFF
cmake --build /private/tmp/cmqt-parent-framework-only -j 4
cmake --build /private/tmp/cmqt-parent-framework-only --target all_qmllint
QT_QPA_PLATFORM=cocoa /private/tmp/cmqt-parent-build/tests/CaliburnQmlTests
/private/tmp/cmqt-parent-build/bin/CaliburnExampleApp.app/Contents/MacOS/CaliburnExampleApp
```

## 自动检查结果

最终全部 25 个 CTest 入口通过：新增 parent_protocol、原有 6 个功能入口和 18 个独立 Bootstrapper 入口。以下 Qt Test 数量包含 init/cleanup，不代表断言数量。

| 检查 | 实际结果 |
| --- | --- |
| 独立配置及全量构建 | 通过；接口、模板层、moc 继承链、QML 注册及静态插件均编译成功。 |
| parent_protocol | 17 条通过，覆盖接口转换、只读 Parent、类型约束、自定义 IChild、单项停用恢复、多项留存、混合父关闭、意外销毁、父树提前回收、嵌套关系和结果信号。 |
| collection | 20 条通过，回归接管、选择、普通 VM、快照、相邻关闭、父关闭及意外销毁。 |
| counterservice | 56 条通过，边界及通知行为保留。 |
| conductor | 28 条通过，原接管、切换和生命周期行为回归。 |
| core | 67 条通过，原属性、业务操作及元对象断言回归。 |
| composition | 13 条通过，Home/Detail 工厂、服务共享与隔离、页面及服务单次回收回归。 |
| qml（offscreen） | 27 条通过；新增 ConductorBase 不可创建、QML 只读 Parent、单项停用卸载 View、恢复重建及关闭时 View 先于 VM 销毁，集合导航 Parent 断言通过。 |
| Bootstrapper | 18 个入口通过，覆盖启动、异常清理、退出和实际示例导航退出场景。 |
| all_qmllint | 框架和用户模块均通过。 |
| 仅框架构建及 qmllint | 通过；生成的 build.ninja 中没有 Boost DI 或 CaliburnExample 目标。 |
| Cocoa 原生 QML 测试 | 27 passed、0 failed、0 skipped，退出码 0。 |

Parent 顺序测试验证：单项替换先清旧 Parent、设新 Parent，再通知选择、关闭旧项、激活新项、发布处理结果；集合添加及关闭在成员和选择通知前更新 Parent；父关闭混合当前项和留存项时一次清空所有关系，再通知并按接管顺序关闭，删除事件后各释放一次。

首轮 QML 测试发现两处测试适配问题：新类型的不可创建提示未匹配既有“创建”断言，测试绑定在 VM 删除后未处理空值。分别明确不可创建文案、为测试绑定增加空值判断后回归通过。随后补充混合父关闭和集合 Parent 顺序测试，最终全量再次通过。故意非法夹具诊断、静态插件重复链接提示及 Cocoa 输入法日志不作为通过证据，未抑制警告绕过问题。

## 实际窗口回归

使用完整应用路径绑定本轮独立构建窗口，通过原生 UI 操作检查：

| 操作 | 观察结果 |
| --- | --- |
| 启动 Home | Shell/Home 初始化且活动，Detail 无页面，计数 0。 |
| 点击增加、加 2，文本框输入 parent2，按 Tab | 计数 3；文本数字不改变计数；Tab 切到增加按钮。 |
| 查看详情 | Home 非活动，Detail 活动，显示共享计数 3，导航可用状态正确。 |
| 返回首页后按数字键 2 | 文本框恢复空值，计数先保留 3、再变为 5；页面获得焦点，增加和加 2 禁用。 |
| 重置后在文本框按 2 | 计数 0、文本 2，输入未触发页面加数。 |
| 点击页面空白后按 2 | 页面焦点恢复，计数变为 2。 |
| 再次查看详情 | 显示最新共享计数 2，Home/Detail 生命周期文字正常。 |
| 从 Detail 关闭窗口 | 本轮直接启动的进程退出码 0。关闭后通过进程结果验证，没有再读取可能自动启动应用的 UI 状态。 |

最初使用应用名绑定时，关闭后 UI 工具打开了另一个路径的同名示例。因此重新使用 /private/tmp/cmqt-parent-build/bin/CaliburnExampleApp.app 的完整路径绑定，重复上述操作，并以本轮启动进程的退出码确认结果。

## 边界与历史记录

本轮只支持主线程同步、非重入操作；没有异常恢复或跨 Conductor 转移接口。逻辑 Parent 不等于所有权清单，单项留存项不在 getChildren 中但仍有 Parent。CM 单项普通停用检查关闭策略，本轮 Qt 不检查，相关设计仍在 ToDoList。

历史 [4A](Conductor核心验收记录.md)、[4B](4B验收记录.md)、[4C](4C验收记录.md) 记录未修改。麒麟尚未执行本轮构建与窗口操作，不能视为通过；外部消费工程接入也未在本轮验证。
