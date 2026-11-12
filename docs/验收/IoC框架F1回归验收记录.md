# IoC 框架 F1 回归验收记录

日期：2026-11-12。源码基线：`3059d1a0b60fadbcaa8849ba807254fcec07399e`。对应 [IoC 改造方案](../IoC改造方案.md)第 6 步。

## 环境与验证范围

| 项目 | 实际配置 |
| --- | --- |
| 操作系统 | macOS 27.0.1，构建号 26A434 |
| 主机架构 | arm64 |
| Qt | `/Users/guhui/Qt/6.8.3/macos`，版本 6.8.3（qmake -query 确认） |
| 编译工具 | Apple clang 21.0.0，CMake 3.22.1，Ninja 1.12.1 |
| 构建模式 | Debug，显式 `CMAKE_OSX_ARCHITECTURES=arm64` |
| GUI 自动化环境 | QT_QPA_PLATFORM=offscreen、QT_QUICK_BACKEND=software、QT_QUICK_CONTROLS_STYLE=Basic |

开始时工作区干净，以下三个目录均不存在：

- `build/ioc-f1-regression`：框架、全部测试及原示例。
- `build/ioc-f1-framework-only`：关闭示例及测试的纯框架。
- `build/ioc-f1-standalone`：examples/standalone 独立顶层消费工程。

使用全新目录重新生成和编译，不复用此前 Debug、纯框架或消费工程的对象文件。保持现有源码及旧根工厂调用方式，不迁移示例到提供器，不新增功能、不修改下游依赖配置。

## 命令与日志

在仓库根目录执行，配置使用 Qt 路径 `/Users/guhui/Qt/6.8.3/macos`。

```sh
cmake -S . -B build/ioc-f1-regression -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH=/Users/guhui/Qt/6.8.3/macos \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCALIBURN_BUILD_EXAMPLE=ON -DCALIBURN_BUILD_TESTS=ON
cmake --build build/ioc-f1-regression --parallel 4
ctest --test-dir build/ioc-f1-regression --output-on-failure
cmake --build build/ioc-f1-regression --target all_qmllint
ctest --test-dir build/ioc-f1-regression -R '^bootstrapper_example$' -V

cmake -S . -B build/ioc-f1-framework-only -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH=/Users/guhui/Qt/6.8.3/macos \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCALIBURN_BUILD_EXAMPLE=OFF -DCALIBURN_BUILD_TESTS=OFF
cmake --build build/ioc-f1-framework-only --parallel 4
cmake --build build/ioc-f1-framework-only --target all_qmllint

cmake -S examples/standalone -B build/ioc-f1-standalone -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH=/Users/guhui/Qt/6.8.3/macos \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCALIBURN_SOURCE_DIR=/Users/guhui/Githubs/caliburn-micro-qt \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build build/ioc-f1-standalone --parallel 4
cmake --build build/ioc-f1-standalone --target all_qmllint
ctest --test-dir build/ioc-f1-standalone --output-on-failure
```

命令由忽略目录 `build/ioc-f1-logs/run_verification.py` 调度，逐项记录命令数组、退出码、超时及耗时。结果保存在 `results.json`，每条命令分别保存 `<步骤名>.stdout.log`、`<步骤名>.stderr.log`。该脚本及日志不纳入版本控制。

首次完整联网配置在 Boost.DI 下载阶段达到 180 秒超时，保留 regression-configure 的 stdout、stderr 和 timed_out 记录，无正常退出码。终止本任务遗留的下载子进程后，按照现有《日志接入》的离线构建方式，复制已缓存的固定版本头文件到 `build/ioc-f1-logs/boost-di-offline/di.hpp`，校验后追加以下参数重新配置：

```sh
cmake -S . -B build/ioc-f1-regression -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH=/Users/guhui/Qt/6.8.3/macos \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCALIBURN_BUILD_EXAMPLE=ON -DCALIBURN_BUILD_TESTS=ON \
  -DFETCHCONTENT_SOURCE_DIR_BOOST_DI=/Users/guhui/Githubs/caliburn-micro-qt/build/ioc-f1-logs/boost-di-offline
```

Boost.DI v1.3.2 头文件 SHA-256 为 `1680ca33ffa04457edcea5c14346ad6e3138d8bb34d4fe074a59e13e34b54827`，与仓库固定值一致，项目配置也会再次检查。spdlog v1.17.0 在本轮成功联网下载，归档 SHA-256 为 `d8862955c6d74e5846b3f580b1605d2428b11d97a410d86e2fb13e857cd3a744`，与固定值一致。

没有升级依赖、改用系统包或修改仓库依赖设置。使用的是缓存头文件源码，不是旧对象文件；本轮依然从零编译。无法据此宣称首次联网下载已通过。

重试调度脚本为 `build/ioc-f1-logs/retry_regression.py`，结果写入 `retry-results.json`，保留原始 results.json 中的失败和其他独立检查。两份结果共同构成本轮证据；各成功命令退出码为 0。

## 全量回归与原示例

| 检查 | 实际结果 |
| --- | --- |
| 全新完整配置与构建 | 校验后使用 Boost.DI 源码缓存配置成功，3.48 秒；从零构建成功，72.27 秒 |
| 完整 CTest | 153/153 通过，CTest 报告 35.40 秒，退出码 0 |
| 完整工程 all_qmllint | 通过，退出码 0 |
| minimal 实际 AppBootstrapper 集成测试 | bootstrapper_example 1/1 通过，0.94 秒，退出码 0 |
| multiview 应用二进制自动关闭 | --auto-close 500，0.87 秒正常结束，退出码 0，无超时 |
| multiscreen 应用二进制自动关闭 | --auto-close 3，3.34 秒正常结束，退出码 0；主辅窗口首帧及预览模式检查通过，无超时 |
| 应用和测试程序架构 | 三个示例应用及 CaliburnBootstrapperProviderTests 均为 Mach-O 64-bit arm64 |

minimal 构建完整应用二进制，启动退出通过既有 bootstrapper_example 集成测试验证：运行实际 AppBootstrapper，检查 Home 和 Detail 的装配、共享计数以及正常关闭。未为 minimal main 新增自动关闭功能，也没有运行桌面人工交互，不能将该结果表述为人工启动独立应用主入口验证。

multiview 和 multiscreen 使用原应用二进制分别传入 `--auto-close 500`（毫秒）与 `--auto-close 3`（秒），由 Python subprocess 设置 20 秒超时，并保存输出和退出码。超时属于失败，不将强制终止算作正常退出。multiscreen 在本环境验证预览模式，不是实际四屏或硬件渲染验收。

## 纯框架与独立消费工程

| 检查 | 实际结果 |
| --- | --- |
| 纯框架配置、构建、QML 检查 | 均通过，退出码均为 0；构建 12.84 秒 |
| 纯框架无示例、测试及 Boost.DI／spdlog 目标 | 通过，实际目标清单未匹配禁止目标 |
| 独立消费工程配置与构建 | 通过，退出码均为 0；构建 25.95 秒 |
| 独立消费工程 all_qmllint | 通过，退出码 0 |
| standalone_consumer | 1/1 通过，CTest 报告 1.24 秒，退出码 0；应用与测试二进制均为 arm64 |
| 独立消费工程无示例与第三方目标 | 通过，原有 CMake 防误引入检查及实际目标清单检查均通过 |

目标检查使用 `ninja -t targets all` 保存实际构建目标清单。standalone 本身允许消费测试目标；禁止的是框架示例、Boost.DI、spdlog。其原有 CMake 检查在检测到禁止目标时会令配置失败。本轮使用该原有检查，不修改消费源码。

## 失败、限制与结论

F1 本机自动化回归完成：全新完整构建、153 项测试、框架和示例 QML 检查、原示例启动退出、纯框架及独立消费工程检查全部通过。原示例源码未改动，旧根工厂方式继续工作。本轮没有发现需要修复的生产缺陷，未修改代码或构建清单，只有验收文档及方案状态变更。

唯一失败为首次 Boost.DI 联网下载超时，已保留原始记录；使用与固定版本 SHA-256 完全一致的源码缓存后，全新配置和编译通过。因此本轮证明本机源码兼容、构建和运行行为，不证明该网络环境中的首次依赖下载可用。构建日志中未检出 warning: 或 error: 诊断，QML 检查无错误。

本记录不覆盖第 7 步红厅 F2；真实桌面人工操作、现场四屏和麒麟均未执行，不承诺二进制 ABI 兼容。本轮不提交推送，历史验收正文保持原状。
