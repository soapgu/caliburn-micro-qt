# IoC 旧应用 F2 兼容验收记录

日期：2026-11-12。对应 [IoC 改造方案](../IoC改造方案.md)第 7 步。

当前状态：后续修正测试准备条件后，完整应用回归 79/79 通过，本机旧源码重建与模拟运行兼容验收通过。下文保留初次验收过程及原结论，最新结果见文末复验章节。

## 基线与隔离方式

| 项目 | 本轮基线 |
| --- | --- |
| 红厅应用 | `/Users/guhui/GitLabs/project_expo/red-conf-system`，`505d5fa91dc1d605c838eff412bdd67a45610ca5` |
| Qt 框架 | `/Users/guhui/Githubs/caliburn-micro-qt`，`ed3b0f0e63207820fe3d9bc91448f9eface6a36e` |
| 初始工作区 | 两个仓库均干净 |
| 新构建目录 | 红厅仓库的 `build/ioc-f2-compat`，开始时不存在 |
| 环境 | macOS 27.0.1／26A434，原生 arm64，Qt 6.8.3，Apple clang 21.0.0，CMake 3.22.1，Ninja 1.12.1 |

应用源码、原有依赖配置、`GIT_TAG main`、旧 build/debug 及其依赖缓存均不修改。通过 FetchContent 的命令行源码目录覆盖使用本地框架；配置生成的 caliburn-revision.txt 必须与目标完整 SHA 一致。框架示例和测试继续由应用原配置关闭。

执行使用 `/usr/bin/arch -arm64` 和显式 CMAKE_OSX_ARCHITECTURES=arm64；当前无受限沙箱，不使用 Rosetta、不跳过 Qt CPU 特性检查。未部署麒麟，未接入新提供器或迁移应用构造方式。

## 构建命令、依赖与日志

在红厅仓库根目录执行：

```sh
/usr/bin/arch -arm64 cmake -S . -B build/ioc-f2-compat -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH=/Users/guhui/Qt/6.8.3/macos \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DBUILD_TESTING=ON \
  -DFETCHCONTENT_SOURCE_DIR_CALIBURN_MICRO_QT=/Users/guhui/Githubs/caliburn-micro-qt \
  -DFETCHCONTENT_SOURCE_DIR_BOOST_DI=/Users/guhui/GitLabs/project_expo/red-conf-system/build/ioc-f2-logs/boost-di-offline \
  -DFETCHCONTENT_SOURCE_DIR_SPDLOG=/Users/guhui/GitLabs/project_expo/red-conf-system/build/ioc-f2-logs/dependencies/spdlog/spdlog-1.17.0 \
  -DFETCHCONTENT_SOURCE_DIR_QXLSX=/Users/guhui/GitLabs/project_expo/red-conf-system/build/ioc-f2-logs/dependencies/qxlsx/QXlsx-1.5.1.1 \
  -DFETCHCONTENT_SOURCE_DIR_LIBXLS=/Users/guhui/GitLabs/project_expo/red-conf-system/build/ioc-f2-logs/dependencies/libxls/libxls-1.6.3
/usr/bin/arch -arm64 cmake --build build/ioc-f2-compat --parallel 4
/usr/bin/arch -arm64 ctest --test-dir build/ioc-f2-compat --output-on-failure
/usr/bin/arch -arm64 cmake --build build/ioc-f2-compat --target all_qmllint
```

Boost.DI 使用复制到独立日志目录的固定版本缓存头文件，SHA-256 为 `1680ca33ffa04457edcea5c14346ad6e3138d8bb34d4fe074a59e13e34b54827`。其他依赖保持应用原固定版本及校验，不升级、不替换成系统包。首次配置没有上述三个归档源码覆盖参数，在 QXlsx 下载阶段达到 180 秒超时，退出码 -9，原始日志保留。随后只读校验旧 build/debug 中的三个固定版本归档，解压到本轮独立日志目录，再以这些源码目录重试，配置 4.63 秒成功；旧缓存没有改动。缓存仅用于源码，所有本轮对象文件在新目录重新编译。

用户随后要求使用 HTTP 代理。读取当前系统设置后，使用 `http://127.0.0.1:7897`，以 HTTP_PROXY／HTTPS_PROXY 和 curl --proxy 显式验证四个上游固定版本下载，保留 TLS 校验，未改系统代理或仓库配置。四项下载均退出 0 且校验通过：

| 依赖 | 固定版本 | SHA-256 | 代理下载耗时 |
| --- | --- | --- | --- |
| spdlog | 1.17.0 | d8862955c6d74e5846b3f580b1605d2428b11d97a410d86e2fb13e857cd3a744 | 6.55 秒 |
| Boost.DI | 1.3.2 | 1680ca33ffa04457edcea5c14346ad6e3138d8bb34d4fe074a59e13e34b54827 | 6.29 秒 |
| QXlsx | 1.5.1.1 | 70e424ad47529e16cd8ad1bad2ccc561af1eb50ef8ee45f1b50cb277f9a12730 | 10.19 秒 |
| libxls | 1.6.3 | 587c9f0ebb5647eb68ec1e0ed8c3f7f6102622d6dd83473a21d3a36dee04eed7 | 6.68 秒 |

代理下载结果与构建所用缓存头文件／归档完全相同，故继续正在进行的源码编译，不中断或重复构建。代理下载的命令和校验结果位于 `proxy-downloads/results.json`，源码归档校验及解压来源记录在 `dependencies/verified.json`。

调度脚本和日志位于红厅仓库忽略的 `build/ioc-f2-logs/`。results.json 保留首次配置超时，retry-results.json 记录缓存源码重试及后续逐项命令数组、退出码、超时和耗时；每条命令分别保存 stdout 和 stderr。脚本对子进程设置独立进程组，超时时终止本次命令及其子进程，超时不能算作正常退出。

## 本机构建与测试结果

| 检查 | 实际结果 |
| --- | --- |
| 实际框架 SHA | 配置输出已确认与目标完整 SHA 一致 |
| 全新 Debug 构建 | 通过，退出码 0，234.32 秒，完成 499 个 Ninja 构建步骤 |
| 完整应用 CTest | 78/79 通过，301.70 秒，退出码 8；client_voteDevices 有两条断言失败，详见下节 |
| all_qmllint | 通过，退出码 0 |
| 原生程序架构 | RedConfClient、RedConfClientTests、RedConfConferenceBrowserTests 均为 Mach-O 64-bit arm64 |
| 未引入框架示例目标 | 通过；原有 CMake 检查及实际 Ninja 目标清单均未发现框架示例或框架测试目标，应用自身 Boost.DI 目标按原设计保留 |

以下相关场景均在本次完整测试中通过，未为汇总重复运行：

| 相关范围 | 通过的 CTest 场景 |
| --- | --- |
| 装配与共享服务 | client_composition、client_conferenceSession、client_checkInReader、client_voteReader |
| 根和 ViewHost 装配／渲染 | client_shellRendering、client_shellLayouts、client_shellLayoutLifecycle、client_multiViewPresentation |
| 输出装配与回收 | client_outputFramework、client_outputScreens、client_outputFullRefresh、client_outputLifecycle、client_outputRendering、client_outputLoadFailure |
| 启动失败回滚 | client_startupFailureRollback |
| 主辅窗口与退出 | client_windowsAndExit_auxiliary、viewModel、primary、early、auto、destroy、maximized（后六项同样以 client_windowsAndExit_ 为前缀） |
| 弹窗及页面交互 | client_conferenceBrowser、client_conferenceEditor、client_seatAssignment、client_seatRelease、client_seatAdjustment |

完整 QtTest 输出未发现 SKIP 记录。通过项目不抵消失败项目，完整测试仍判定未通过。

构建输出包含两条链接器 duplicate libraries 警告（RedConfCheckInSources，以及 QXlsxQt6／RedConfExcel），没有因此修改源码或链接配置；构建退出码为 0。

## 实际应用启动与退出

仅调整新构建应用目录中的 config.json：vote、checkin、signal 的 mock 均设为 true，日志 level=info、cm_log=true。原仓库配置中 vote.mock=false；测试配置的差异明确记录，不用于声明真实表决设备兼容性。

由于既有 CLI 测试同样启动应用，构建完成后先将该产物配置切换为 mock，再执行完整 CTest，应用正常启动验证前再次确认配置。仅修改构建产物，仓库根配置、原运行目录及设备凭据不变。

正常应用二进制以 offscreen、software、Basic 环境运行，参数为 `--auto-close 5`，Python subprocess 限制 30 秒。会议文件默认位置是新应用目录下的“会议文件”，不复用旧运行目录。验证证据包括预览模式、主窗和矩阵输出首帧、窗口校验及退出码。

| 检查 | 实际结果 |
| --- | --- |
| 主窗／矩阵输出创建 | 通过，正常进入 preview 模式；表决源 MockVoteSource、信号源模拟，报到配置为 mock |
| 两个窗口首帧与预览校验 | 通过，日志记录“主窗口首帧已提交”“矩阵输出窗口首帧已提交”和 Window verification passed; mode: preview |
| 自动关闭及退出码 | --auto-close 5，6.15 秒正常退出，退出码 0，无超时 |
| 配置隔离与工作区复核 | 模拟配置仅位于新产物目录；实际运行及旧框架对照结束后，红厅及旧框架缓存的 git status --short 均为空，应用 HEAD 保持原基线 |

## 失败项目及对照定位

完整测试的唯一失败项为 client_voteDevices，包含两条失败断言：

- authorizationSummary：tests/tst_votedevices.cpp:195，期望摘要以“已赋权 1”结尾，实际没有增加。
- voterDetails：tests/tst_votedevices.cpp:214，期望 authorizedVoterCount 为 1，实际为 0。

日志显示相关命令只进入“离线缓存”，发送通道数和表决器数均为 0。静态核对 DeviceVoteSource::advanceAuthorization 会跳过离线控制器，只有 sendAuthorization 实际调用 setVoterEnabled 时才更新 m_enabledVoters；这与应用 AGENTS.md 的“排队和设备回执不改变计数”约定一致。两个失败用例在控制器在线事件之前就期待计数为 1，且这些断言不经过 Bootstrapper 的启动入口。

为排除完整测试顺序干扰，使用新构建单独运行 authorizationSummary 和 voterDetails，仍有两条失败，进程退出码 2。随后在独立 `build/ioc-f2-old-framework-control` 目录，用同一应用源码、相同 Qt／依赖／编译选项，仅将框架来源改为原有缓存中干净的旧框架 `00b72ae7ee69c8ecc9d4b46c53066a1fb51aa460`，只重建 RedConfVoteDevicesTests 并运行同样两个用例；不修改或复用旧 build/debug 的对象文件。

旧框架对照配置通过，目标重建 91.10 秒完成；两个用例同样在第 195、214 行失败，进程退出码 2，分别表现为摘要未达到“已赋权 1”和实际计数 0／期望 1。新框架单独运行 0.29 秒、旧框架单独运行 0.24 秒，均未超时。该对照使用相同应用基线从源码重新编译，不依赖旧测试二进制的来源推断；检查旧框架缓存 git status 仍为空。

结合实际代码及项目约定，这两条失败属于既有测试预期与离线赋权语义不一致，不是本轮 IoC 改造新增的回归。没有因此修改测试断言或生产行为。建议后续单独修正测试准备条件：先验证离线排队计数仍为 0，再模拟控制器在线及实际下发后验证计数 1，继续验证 stop 不清理已下发赋权；修正后再重跑完整应用回归。

对照命令和结果保存在 control-results.json 及 control-*.stdout.log／stderr.log；旧框架实际 SHA 记录在独立对照目录的 caliburn-revision.txt。

## 限制与结论

F2 已执行但未通过，不能标记完成：全新构建、QML 检查和实际应用主辅窗口启动退出已通过，但完整应用测试尚有失败。未为了通过验证而修改红厅业务代码或测试，也未发现已确认需要修复的框架兼容缺陷。

本轮验证旧源码重新构建及本机模拟运行，不承诺预编译二进制 ABI 兼容。真实桌面、真实设备、硬件渲染、现场四屏及麒麟未验证。未提交推送，历史验收正文保持原状。

## 2026-11-12 测试修复后复验与收尾

初次 78/79 结果及旧框架对照结论保留如上。随后应用以独立提交 `8061cf2`（修复表决设备赋权测试的上线前置条件）修正两个测试的准备条件：启动设备源监听，先验证离线排队不计数，再模拟控制器上线并等待实际下发，验证赋权数为 1，继续验证停止后保留赋权。仅修改 `tests/tst_votedevices.cpp`，未修改生产代码、构造方式或框架实现。

| 项目 | 复验结果 |
| --- | --- |
| 应用修复提交 | `8061cf2`，已提交推送 |
| 验收框架 SHA | `ed3b0f0e63207820fe3d9bc91448f9eface6a36e`，复验时独立构建目录 caliburn-revision.txt 记录的版本 |
| 构建与运行环境 | 沿用本记录的独立 build/ioc-f2-compat，macOS 原生 arm64 |
| 完整应用 CTest | 79/79 通过，0 失败，301.99 秒 |
| 表决设备场景 | client_voteDevices 通过，6.25 秒 |
| 日志 | 应用仓库 `build/ioc-f2-logs/fix-ctest.log`；构建日志 `build/ioc-f2-logs/fix-build.log` |

复验命令为 `/usr/bin/arch -arm64 cmake --build build/ioc-f2-compat --target RedConfVoteDevicesTests --parallel 4`，随后执行 `/usr/bin/arch -arm64 ctest --test-dir build/ioc-f2-compat --output-on-failure`。日志末尾明确记录 100% tests passed、0 tests failed out of 79。该日志及关联聊天记录是本次补记依据，本次文档收尾没有重复运行完整 F2 测试。

结合初次全新构建、QML 检查、模拟主辅窗口首帧及正常退出证据，以及修复准备条件后的完整回归，F2 更新为：**本机旧源码重建与模拟运行兼容验收通过**。应用继续使用旧根工厂及原构造方式，尚未迁移生产创建器或启用新提供器。兼容结论允许单独修正既有测试前置条件，不表示原有两条失败断言曾经通过。

本结论不包含真实设备、真实桌面硬件渲染、现场四屏及麒麟平台验证，也不承诺二进制 ABI 兼容。下一阶段可建立最小应用容器适配，再单独进行末端创建器试点。

补记：后续最小应用适配验证复用了 build/ioc-f2-compat，配置重新生成的框架版本为 `e32a500353048d27dd0039549cf124b4fa7fb9d1`；相对上述 F2 框架仅有文档变更。该后续验证与本节 79/79 历史复验分开记录，不能以当前缓存版本替代当时验收版本。
