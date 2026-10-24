# Multi-View 最小示例

这是项目自建的 Qt/QML 演示，不是 CM 官方案例移植。框架 context 定位行为参考 CM 3.2.0 源码，接口与差异见 [Multi-View](../../docs/Multi-View.md)。

ShellViewModel 只持有一个 HomeViewModel；左侧 ViewHost 固定 Master，右侧初始 Detail，可切换到 Master 或默认 View。两边引用同一 VM，修改名称或增加计数立即同步；临时备注只属于当前 View，切换后清空。切换不重新创建 VM，也不驱动 VM 生命周期。

资源布局为 `views/ShellView.qml`、`views/HomeView.qml`、`views/Home/Master.qml` 和 `views/Home/Detail.qml`，在 Bootstrapper 配置搜索根与 MultiView 命名空间前缀，不逐项登记视图。Master/Detail 名称只是两种展示上下文，不代表两个 VM 或一套客户主从业务。

```bash
cmake --preset debug
cmake --build --preset debug --target CaliburnMultiViewApp
ctest --preset debug -R '^(multiview_|viewlocator_context)' --output-on-failure
```

Qt Creator 选择 CaliburnMultiViewApp 运行；macOS 可执行文件为 `build/debug/bin/CaliburnMultiViewApp.app/Contents/MacOS/CaliburnMultiViewApp`，Linux 为 `build/debug/bin/CaliburnMultiViewApp`。可用 `--auto-close 2000` 在两秒后请求关闭窗口，供启动验证使用。

手工验证：左侧编辑名称、点击计数，检查右侧同步；在右侧填写临时备注，再切换 context 并返回，检查备注清空而共享名称/计数保留。左侧 View 不随右侧切换重建。关闭主窗口或点击退出应正常退出。
