# 泛型单项 Conductor 核心验收记录

2026-10-07 完成本轮核心实现与自动验收。macOS arm64 / Qt 6.8.3 全量构建、全部 CTest、qmllint 和仅框架构建通过。Shell/Home 示例保持原行为；Collection.OneActive 下一轮实现，第四批示例导航尚未完成。未暂存、提交或推送。

## 交付与环境

- 非模板 ConductorViewModelBase 提供 QML 只读 activeItem 与通知，模板 Conductor<T> 提供 C++ 类型约束，T 为 ViewModelBase 派生类。
- 当前项由 Conductor 接管；切换先通知新项，再关闭旧 Screen、按父状态激活新项，最后 deleteLater。普通 VM 不执行 Screen 生命周期。
- 父停用保留选择；未初始化父对象关闭跳过生命周期并保留当前项，已初始化父对象关闭清空选择、关闭当前子项并延迟回收；父对象重新激活时需要重新创建子页面，析构不补关闭。
- Screen 关闭条件对齐 CM，删除关闭标记与待清理资源扩展；已初始化对象重复关闭仍执行钩子，未改变的状态不重复通知。
- Screen 与 Conductor 统一移除转换校验、重入标记及恢复 guard；生命周期和导航由调用方保证在应用主线程执行，嵌套操作留待具体需求处理。
- 接管校验失败保留调用方 unique_ptr；候选项同线程、无父对象、非活动 Screen、自身和祖先约束均验证。
- 不提供关闭守卫、异步生命周期、Items 集合或页面缓存；延迟删除是明确的 Qt 所有权适配，不等同于 CM 的 GC 行为。

环境为 macOS 27.0.1 arm64、Qt 6.8.3、AppleClang 21.0.0、CMake 3.30.5、Ninja、C++17。使用已有 macos-local 私有预设，构建产物位于独立临时目录；未修改公共预设。

## 命令与结果

```sh
cmake --preset macos-local -B /private/tmp/cmqt-conductor-build
cmake --build /private/tmp/cmqt-conductor-build -j 4
ctest --test-dir /private/tmp/cmqt-conductor-build --output-on-failure
cmake --build /private/tmp/cmqt-conductor-build --target all_qmllint -j 4

cmake --preset macos-local -B /private/tmp/cmqt-conductor-framework-only \
    -DCALIBURN_BUILD_EXAMPLE=OFF -DCALIBURN_BUILD_TESTS=OFF
cmake --build /private/tmp/cmqt-conductor-framework-only --target all all_qmllint -j 4
```

| 检查 | 结果 |
| --- | --- |
| 独立目录全量配置与构建 | 框架、静态插件、原示例及全部测试完成构建。 |
| 全部 CTest | 22 个测试入口全部通过，无失败。 |
| conductor | 28 条通过，包括数据行和初始化/清理，无失败或跳过；目标直接依赖框架与 Qt Test。 |
| 核心与装配回归 | core 66 条、composition 4 条通过，无失败或跳过。 |
| QML | 20 条通过，包括 Conductor 不可创建类型和 ViewHost 集成，无失败或跳过。 |
| Bootstrapper | 18 个独立测试入口全部通过，含示例启动与退出。 |
| qmllint | 框架及原 Shell/Home 示例均通过。 |
| 仅框架 | 独立目录的静态库、插件及 qmllint 通过，未生成示例或测试目标。 |

泛型测试含具体类型返回值和编译期接管约束；接管失败测试验证原 unique_ptr、父关系及当前项不变。保留异线程候选项接管拒绝测试，候选项在自身线程创建和销毁，拒绝后调用方仍持有所有权。删除 Screen/Conductor 的调用线程拒绝和重入拒绝测试及其专用回调辅助代码。

生命周期测试验证 A→B 的顺序为 changed、A.close、B.initialize、B.activate，以及通知时旧项仍活动、新项尚未初始化。清空和显式关闭也先通知、后关闭；延迟删除前保留父关系，处理事件后恰好回收一次。父树先销毁时同时回收当前项和待删除旧项，不重复删除；嵌套 Conductor、父停用/恢复/关闭均通过。父关闭补充普通 VM 与 Screen 的十组状态组合（未初始化、仅初始化、活动、停用、已关闭后接入新项），验证未初始化父对象重复关闭不清空、不通知、不触发生命周期或延迟删除，后续初始化后可正常关闭；其余状态验证清空通知先于子关闭、延迟释放，以及重复关闭不重复通知或删除已移除的子项。另验证未初始化父对象跳过关闭后由父树恰好回收子项一次、不补生命周期；core 回归验证普通 Screen 与 Shell/Home 已初始化后重复关闭仍执行钩子、初始化及活动状态通知次数不变。父关闭后、删除事件前销毁父对象，仍只回收子项一次。

QML 使用业务 QObject 继承 Conductor<HomeViewModel> 的具体子类，验证模板层与 moc 的配合。宿主以 required ConductorViewModelBase 属性绑定 activeItem，从嵌入资源加载 HomeView；替换、显式关闭当前项、父关闭、当前项意外销毁后视图更新正确，旧 View 在旧 VM 之前销毁，正常路径引擎无警告。

## 限制与诊断

沙箱内首次配置时 Qt 工具无法正确检测 ARM NEON，配置中止；同一配置在沙箱外通过，此后 Qt 构建与测试在沙箱外执行。未修改 Qt 安装或关闭其 CPU 校验。

已有故意非法 QML 夹具在导入扫描和失败路径测试中产生预期诊断；静态插件链接仍有重复库提示。离屏字体别名提示不影响测试结果。

本轮没有新增实际导航窗口，不把此前 Cocoa/人工窗口验收作为新 Conductor 的人工验收。麒麟、Linux/GCC 本轮运行、Collection.OneActive 和第四批示例导航仍未验证。
