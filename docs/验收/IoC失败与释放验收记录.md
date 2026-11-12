# IoC 失败与释放验收记录

日期：2026-11-12。基线提交：`8a51712008c062043c0e3963c319d3adbececf13`。对应 [IoC 改造方案](../IoC改造方案.md)第 5 步，接续 [Bootstrapper 提供器接入验收](Bootstrapper提供器接入验收记录.md)。

## 范围与变更

本轮仅扩展 `tests/cpp/tst_bootstrapper_provider.cpp`、CTest 场景登记及文档。未修改生产实现、公开 API、WindowManager 清理流程、业务代码或下游应用；未恢复已移除的根实际类型校验。

既有 30 个提供器场景继续覆盖配置失败、配置异常、解析缺失／异常、服务能力和线程约束、根空结果及错误契约、基础显示失败、旧工厂优先和清理异常。本轮在相同程序内新增 12 个独立场景，总计 42 项提供器 CTest，完整测试共 153 项。

## 观测方式与共同断言

测试夹具新增根创建、激活、关闭尝试、关闭钩子和析构计数，以及 `release.complete` 事件，分别表示开始释放与实际完成资源释放。根在激活钩子中观测已绑定的窗口，故激活抛异常前创建的部分 View 也能记录。

根析构只记录状态，不执行 QtTest 断言：检查服务的 windowFor(this) 已为空、已观测 View 已失效。测试函数随后断言：

- 创建过的受测根最多一个，析构次数等于创建次数；创建未完成时不要求存在根。
- 经激活的根，关闭尝试及关闭钩子各一次；未激活的根不执行关闭钩子。
- 已观测 View 销毁一次且早于根析构；已接管根在窗口释放完成后才析构，根早于提供器析构。parent／线程非法的根在接管前局部回收，不要求等待最终窗口释放。
- 激活失败时部分 View 在 onExit 前释放；其他已显示路径 onExit 先于 View 销毁。onExit 均执行一次。
- run 返回后服务及提供器的框架引用释放，Qt 退出策略恢复，无残留窗口；重复 run 被拒绝。
- Bootstrapper 销毁后处理排队事件和 DeferredDelete，再比较解析、创建、退出、生命周期、释放、订阅及事件计数，均不得增长。

保留服务场景额外检查服务不忙、currentDialog 为空、根关联为空。根原地址仅作为 windowFor 的身份查询值，不解引用失效对象。主动发出清理失败信号后，旧订阅通知计数不增长；最后释放外部服务引用并确认 weak_ptr 失效。

## 新增场景与释放矩阵

以下“根／激活／关闭／析构”是每次运行的受测 ProviderRoot 计数；正常成功运行均为 `1/1/1/1`。关闭计数同时检查尝试信号与钩子次数。

| 新增场景 | 预期退出码 | 根／激活／关闭／析构 | 释放及恢复验证 |
| --- | --- | --- | --- |
| serviceWrongContract | 1 | 0/0/0/0 | 窗口解析返回错误契约；不进入 onStartup，不创建根，不回退默认服务 |
| activateThrows | 1 | 1/1/1/1 | 激活钩子抛异常，已创建 View 在 onExit 前回收，根不重复关闭 |
| startupAfterDisplayThrows | 1 | 1/1/1/1 | 根显示成功后抛异常，不进入事件循环；onExit 后继续完成窗口及根释放 |
| rootCloseThrows | 1 | 1/1/1/1 | 根关闭钩子抛异常，不跳过 onExit，不重复尝试根关闭 |
| exitThrows | 1 | 1/1/1/1 | onExit 抛异常，仅调用一次，窗口先于根回收 |
| exitAndReleaseThrows | 1 | 1/1/1/1 | onExit 与窗口释放均报告异常，服务先完成实际释放再抛错，根与提供器仍回收 |
| retainedShowFailure | 1 | 1/0/0/1 | QML 资源缺失，外部保留服务，但无窗口、弹窗、根关联和旧订阅 |
| retainedStartupFailure | 1 | 1/1/1/1 | 显示后启动异常，外部保留服务不延长 View 或根寿命 |
| retainedExitFailure | 1 | 1/1/1/1 | onExit 异常后服务仍存活，但窗口已释放且订阅已断开 |
| recoverAfterRootFailure | 首次 1，第二次 0 | 首次 0/0/0/0；第二次 1/1/1/1 | 根创建异常后销毁首个上下文，第二个提供器正常启动退出 |
| recoverAfterDisplayFailure | 首次 1，第二次 0 | 首次 1/0/0/1；第二次 1/1/1/1 | 使用 Broken context 制造显示失败，再用有效默认映射成功启动 |
| recoverAfterExitFailure | 首次 1，第二次 0 | 两次均 1/1/1/1 | 首次退出异常不影响第二次解析、退出码或释放过程 |

恢复测试在同一进程内运行两个上下文，两次之间销毁首个 Bootstrapper 并处理排队清理，再创建第二个实例；第二次完成后再次检查首个上下文计数未变。ViewLocator 不重置：根创建和退出失败复用有效映射；显示失败预先登记有效默认映射与缺失资源的 Broken context，首次选择 Broken，第二次选择默认映射。

## 执行环境与结果

macOS arm64，Qt 6.8.3，Debug，复用当前源码匹配的 build/debug。窗口测试沿用 offscreen/software 与 Basic 样式。

在仓库根目录执行：

```sh
cmake --build --preset debug
ctest --test-dir build/debug -R '^bootstrapper_provider_' --output-on-failure
ctest --preset debug
cmake --build build/debug --target all_qmllint
cmake -S . -B build/ioc-contract-framework-only -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH=/Users/guhui/Qt/6.8.3/macos \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCALIBURN_BUILD_EXAMPLE=OFF -DCALIBURN_BUILD_TESTS=OFF
cmake --build build/ioc-contract-framework-only
git diff --check
```

| 项目 | 最终结果 |
| --- | --- |
| Debug 构建 | 通过 |
| 提供器定向 CTest | 42/42 通过，2.72 秒 |
| 完整 CTest | 153/153 通过，34.94 秒；原有 141 项保持通过 |
| all_qmllint | 通过，框架和三个示例模块均通过 |
| 关闭示例与测试的纯框架配置及构建 | 通过，构建为 ninja: no work to do. |
| git diff --check | 通过 |

日志保留在忽略的构建目录 `build/debug/ioc-failure-release/`：build.log、targeted.log、ctest.log、qmllint.log、framework-configure.log、framework-build.log。详细逐场景输出见 `build/debug/Testing/Temporary/LastTest.log`，后续运行会覆盖。

## 失败诊断与交付边界

新增场景首次定向运行全部通过；随后强化了事件循环进入条件、实际窗口释放完成顺序及部分 View 的观测断言，重新执行最终验证。负向场景中的 QCRITICAL、缺失 QML 资源及注入异常日志属于预期触发，成功与否由对应退出码、计数和释放状态断言判断，不按日志级别单独判断。

未发现需要修改生产实现的缺陷，没有生产修复。异常测试遵守服务“失败仍解除借用”的契约，未将遗留借用的非法实现视为框架可兜底的正常服务。

本记录只验收第 5 步的失败与释放专项。第 6 步框架交付回归、第 7 步下游红厅 F2 仍待独立推进；真实桌面、现场四屏、麒麟未执行。不承诺预编译 ABI 兼容，本轮不提交推送。历史验收记录正文保持原状。
