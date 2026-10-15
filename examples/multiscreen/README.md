# CM 双窗口多屏示例

`CaliburnMultiScreenApp` 展示一个主操作窗口和一个扩展显示窗口。两个窗口由同一个 CM 窗口服务管理，分别使用独立 VM、QML 引擎和原生顶层窗口，通过命名空间约定定位 View，不依赖 minimal 示例模块。

默认自动识别显示环境：xcb、四块 1920×1080、DPR=1 且从 `(0,0)` 横向连续排列时进入四屏模式；其他环境进入预览模式。模式写入标题和日志。预览不代表四屏验收通过。

## 构建运行

在仓库根目录设置 Qt kit 并构建：

```sh
export QT_ROOT="你的 Qt kit 根目录"
cmake --preset debug
cmake --build --preset debug --target CaliburnMultiScreenApp
```

macOS：

```sh
./build/debug/bin/CaliburnMultiScreenApp.app/Contents/MacOS/CaliburnMultiScreenApp --auto-close 5
```

Linux：

```sh
./build/debug/bin/CaliburnMultiScreenApp --auto-close 5
```

麒麟图形桌面终端：

```sh
bash examples/multiscreen/run-kylin.sh --strict-screens --auto-close 5
```

脚本默认读取 `~/Qt/qt6-8.3-env.sh`，可通过 `QT_ENV_FILE` 覆盖；使用现有景嘉微 GLVND 环境，不修改系统显示设置。

## 窗口与退出规则

四屏模式主窗在第一屏工作区居中，1440×810、最小 720×480。扩展窗覆盖 `(1920,0,5760,1080)`，内部三等分，使用置顶、X11 绕过窗口管理器、不接受焦点及鼠标穿透配置。

预览模式窗口按第一块屏幕可用区缩放并错开放置；扩展窗有标题栏，可独立关闭，不启用置顶、X11 绕过或鼠标穿透。

- 主窗最小化、最大化、移动或缩放不改变扩展区。
- 关闭辅助窗只释放辅助窗，主窗继续运行，不自动重建辅助窗。
- “退出测试”按钮和自动关闭调用根 VM 的 `tryClose()`；与主窗系统关闭按钮使用同一关闭守卫和生命周期桥接，成功关闭后清理辅助窗并退出。
- 两个示例窗口均无 Esc 退出逻辑。框架既有弹窗的键盘行为不受影响。

## 参数与诊断

- `--strict-screens`：环境不符合四屏条件时直接返回 2。
- `--auto-close <秒数>`：3～86400 秒后请求关闭主窗，无效参数返回 2。
- 视图加载或启动失败返回 1；启动后 1500ms 的首帧/四屏几何检查失败返回 3；场景图渲染失败返回 4。

默认请求硬件 OpenGL；`QT_QUICK_BACKEND=software` 可做软件对照，但不能作为硬件验收依据。应用启动层通过 `windowFor()` 连接渲染诊断和检查几何，业务 VM 不操作原生窗口。辅助窗在检查前被正常关闭时跳过其检查。

自动测试覆盖布局选择、参数、VM 注入、窗口独立关闭、按钮退出、Esc 无动作、自动关闭和启动失败回滚。实际窗口验收见 [多窗口与多屏示例验收记录](../../docs/验收/多窗口与多屏示例验收记录.md)。本次不支持热插拔恢复、混合 DPI 跨屏、长期稳定性或动态刷新同步验收。
