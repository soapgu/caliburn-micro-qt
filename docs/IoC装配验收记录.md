# IoC 装配验收记录

2026-10-06 完成第三批之后的 IoC 装配调整。macOS arm64 自动检查、Cocoa 集成测试和真实窗口操作全部通过；麒麟待验证。未暂存、提交或推送。

2026-10-07 Conductor 核心调整后，已初始化的 Shell/Home 重复关闭仍执行钩子，未改变的活动状态不重复通知；下文幂等描述保留初次验收语义。当前契约及回归结果见 [ScreenViewModel](ScreenViewModel.md) 和 [Conductor 核心验收记录](Conductor核心验收记录.md)。

2026-10-07 随后统一精简 Screen 与 Conductor：移除逐次调用线程检查和重入保护，主线程执行由调用方保证；下文相关防守测试仅代表原验收行为，当前回归结果见 [Conductor 核心验收记录](Conductor核心验收记录.md)。

## 改动范围

- 固定 Boost.Ext.DI v1.3.2 单头文件与 Boost Software License 1.0；配置时核对头文件 SHA-256，不在构建阶段访问网络。
- 新增应用装配静态库 CaliburnExampleComposition，DI 依赖为 PRIVATE。Home 无参构造，Shell 只接收 Home 的 unique_ptr，DI 自动推导依赖；VM 和框架不包含 DI。
- buildShell 递归创建无父 Home/Shell，设置 CppOwnership，返回根 unique_ptr；容器在返回时销毁。
- Shell 接收 Home 的 unique_ptr，验证主线程及无既有父对象，在父关系确认后释放临时所有权；QPointer 保留意外销毁保护。
- Shell 钩子显式驱动 Home 生命周期，main 只驱动根 Shell。页面、注册表和通用 Screen 契约保持第三批行为。
- 新增独立装配测试，并将页面替换测试的 VM 改为从装配后的 Shell 树获取。

## 环境与检查

macOS 27.0 arm64、Qt 6.8.3、AppleClang 21.0.0、CMake 3.22.1、Ninja、C++17，沿用本机 macos-local 预设。

```sh
cmake --preset macos-local -B build/ioc-composition
cmake --build build/ioc-composition
ctest --test-dir build/ioc-composition --output-on-failure
cmake --build build/ioc-composition --target all_qmllint

cmake --preset macos-local -B build/ioc-framework-only \
    -DCALIBURN_BUILD_EXAMPLE=OFF -DCALIBURN_BUILD_TESTS=OFF
cmake --build build/ioc-framework-only --target all all_qmllint

cd /private/tmp
QT_QPA_PLATFORM=cocoa /Users/guhui/Githubs/caliburn-micro-qt/build/ioc-composition/tests/CaliburnQmlTests
```

| 检查 | 结果 |
| --- | --- |
| 新目录配置和全量构建 | 通过；两个 QML 模块、装配库、示例和测试完成构建。 |
| CTest | core、composition、qml 三组全部通过；分别 67、4、16 条，包括数据行及初始化/清理；无失败或跳过。 |
| 核心接管测试 | 非空及无父约束、移交后父树回收、意外删除 Home、Shell 不复制计数、子钩子顺序、停用后关闭与重激活均通过。 |
| DI 装配测试 | 局部容器销毁后树仍可用，两次装配身份独立；父关系、线程、CppOwnership、未初始化初态及 Home 恰好释放一次均通过。 |
| 根生命周期测试 | 只调用 Shell 即完成 Home 初始化/激活/关闭，通知先子后父；重复调用幂等，重激活保留计数。 |
| 回归 | Screen、注册表、宿主加载/替换/失败、计数范围、54 组参数边界、按钮与键盘、文本消费、Tab/空格、VM 替换全部通过。 |
| qmllint | ViewHost、ShellView、HomeView 通过。 |
| 仅框架 | 独立构建及 qmllint 通过；build.ninja 无 boost-di 或装配库依赖。 |
| Cocoa | 从 /private/tmp 运行，16 条全部通过，无失败或跳过。 |
| 实际窗口 | 生命周期文字、参数按钮、键盘边界、文本输入、Tab/空格、页面回焦和正常退出通过。 |

测试从内嵌资源加载 QML，并排除源码与构建目录的 QML 导入副本。故意非法的资源夹具继续产生预期诊断；静态插件链接仍有重复库提示。系统输入法输出 mach port / CapsLock 日志，未影响通过结果。

## 实际窗口操作

从 /private/tmp 启动 build/ioc-composition 中的示例产物，原生窗口操作、可访问状态及截图核对结果：

| 操作 | 结果 |
| --- | --- |
| 启动 | Shell/Home 已初始化、已激活，计数 0。 |
| 点击加 2，再按 2 | 计数 4，加 2 禁用。 |
| 再按 2 | 保持 4。 |
| 重置后在文本框按 2 | 文本为 2，计数 0。 |
| Tab 后按空格 | 焦点到增加，计数 1。 |
| 连续按两次 2 | 计数到 5，增加及加 2 禁用。 |
| 重置、清空文本、点击页面说明区域后按 2 | 页面恢复焦点，计数 2，文本为空。 |
| 关闭窗口 | 应用进程退出码 0。 |

自动重复、修饰键、反向 Tab 和空白鼠标回焦由 offscreen 与 Cocoa 集成测试覆盖，实体长按未单独计时。Conductor、详情导航、服务、弹窗与其他平台继续留在后续批次。

装配接口与所有权说明见 [IoC 与应用装配](IoC与应用装配.md)，第三批原始验收结果保留在 [第三批验收记录](第三批验收记录.md)。

## 构造接口简化复验

同日移除 Home/Shell 构造函数的 parent 参数及 ViewModelInjectionTraits.h，装配代码直接包含 DI 并采用自动推导。非法接管测试改为创建 Home 后通过 setParent 设置已有父对象，继续验证 Shell 拒绝该候选项并安全回收。

重新构建 build/ioc-composition 通过；CTest 三组全部通过（core 67 条、composition 4 条、qml 16 条），qmllint 通过。装配测试继续确认根 Shell 无父对象、Home 归属 Shell、局部容器销毁后树存活、CppOwnership、根生命周期及单次回收。上面的 Cocoa 与实际窗口记录来自同日此前的 IoC 验收；本次构造接口简化通过自动检查复验。
