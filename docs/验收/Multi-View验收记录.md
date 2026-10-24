# Multi-View 验收记录

日期：2026-10-25（Asia/Shanghai）。环境：macOS arm64、Qt 6.8.3、Debug；自动化使用 offscreen/software，桌面 GUI 使用原生 Cocoa。

## 对齐与范围

依据本地 Caliburn.Micro `3.2.0` 标签，提交 `544babd1865846af175ba32d86c839eb354714aa`。已核对 ViewLocator.TransformName、View.OnContextChanged 和 WPF WindowManager 的 context 传递。

本轮完成 context 定位与页面/根窗口/普通窗口/弹窗接入。C++ 使用可空字符串，QML 边界拒绝非字符串对象，属于已确认的 CM object 参数类型收窄。未实现 IViewAware、View 缓存、集合导航 View 保留、Page 后缀和自定义转换规则。独立 Home 示例为项目自建最小演示，不是官方案例移植。完整契约见 [Multi-View](../Multi-View.md)。

## 自动化结果

| 检查 | 结果 |
| --- | --- |
| `cmake --build --preset debug --parallel 4` | 通过；最终构建无新增编译或重复链接库警告 |
| `ctest --preset debug --output-on-failure` | 109/109 通过；原有 94 项与新增 15 项均通过（含独立 context 参数回归） |
| `cmake --build --preset debug --target all_qmllint` | 框架、minimal、multiscreen、multiview 全部通过 |
| 仅框架 Debug 构建及 all_qmllint | 通过；示例与测试关闭，FetchContent 断网模式 |
| 仓库外独立消费工程构建及 all_qmllint | 通过；从 examples/standalone 复制到临时目录，通过 CALIBURN_SOURCE_DIR 接入框架 |
| 仓库外独立消费工程 CTest | 1/1 通过 |

新增定位场景为 contextLayouts、contextMappings、contextValidation、contextScope。验证默认/空字符串键隔离、嵌套 context、显式覆盖与冲突、冻结、精确类型、线程限制、根顺序、命名空间映射及禁止默认回退。qrc 名称大小写严格匹配；本地 file 根沿用文件系统存在性语义，候选名称本身保持原大小写。

新增 multiview 场景覆盖：

- QML 省略、null、undefined、字符串、空字符串，以及拒绝数字/布尔/对象/数组/装箱字符串。
- 同 VM 双宿主、相同 context 不重建、连续往返重建、共享属性刷新、View 局部输入重置、切换不触发生命周期。
- 未匹配、非法 context、缺失 required 属性、非 Item 根、注入不匹配的清空与恢复。
- 宿主销毁、VM 意外销毁、两个受管 View 先于 VM 回收。
- 根窗口、辅助窗口、弹窗 context 命中；弹窗正常关闭结果及 VM 清理、普通窗口关闭守卫。
- 缺失/空字符串/错误根类型的窗口失败，以及弹窗失败 Future、busy 复位与 VM 清理。
- 示例的同一 Home 实例、共享属性同步、context 控件切换和实际 Bootstrapper 自动退出。

旧 core 与 framework_dialog 的精确日志断言已同步新增的 `context: <default>` 字段，未放宽原功能断言。

## 本机 GUI 验证

使用实际 CaliburnMultiViewApp 窗口，通过桌面界面操作验证：

1. 启动后左侧为 Master，右侧为 Detail，窗口布局与输入控件显示正常。
2. 左侧名称改为“共享状态验证”，点击计数按钮；右侧立即显示同一名称及计数 1。
3. 右侧临时备注填写“这段备注应随切换清空”。
4. 右侧改为 Master，再返回 Detail；备注为空，名称与计数仍保留，左侧内容不受切换影响。
5. 右侧选择默认 context，显示默认 View 及保留的共享名称。
6. 点击退出，原运行进程返回 0；日志确认 Shell/Home 正常关闭、窗口服务释放及 Bootstrapper 退出。

桌面输入中文采用粘贴；context 使用下拉框键盘选择。这些是 GUI 工具操作，不宣称为人工手工验收。过程中桌面工具在退出后读取界面时重新打开了示例，原进程退出码与关闭日志单独核验通过。

## 验证边界

本轮未在 Linux、Windows 或麒麟桌面运行，不将本机结果作为这些平台的通过证据；未执行远程 CI。View 状态保留和跨宿主实例复用不在本轮范围，输入重置是当前预期行为。公开 C++ 函数/结构布局与 Qt 元对象签名有变化，下游应重新编译。

## 窗口 context 末尾参数与旧业务兼容复验（2026-10-25）

context 已从窗口与弹窗的 options 结构移除，公开入口为：

```cpp
showWindow(viewModel, options = {}, context = std::nullopt);
showDialogAsync(viewModel, requester, options = {}, context = std::nullopt);
displayRootViewFor<T>(options = {}, context = std::nullopt);
```

context 定位语义参考 CM 3.2.0，Qt 保留弹窗必填 requester 第二位，并将 context 追加到 options 之后以兼容旧业务调用，不声称参数位置与 CM 相同。接口、具体实现、Bootstrapper 和测试替身签名已同步调整；没有兼容重载。省略 options 或保留原有 options 的业务调用无需修改；多屏 AppBootstrapper.cpp 和 minimal HomeViewModel.cpp 已确认相对 HEAD 无差异。公开虚函数签名仍有变化，自定义覆写实现须更新签名，消费工程须重新编译。

新增 windowContextParameters 数据场景，分别通过 IWindowManager 接口和 WindowManager 具体类验证：默认参数生效，显式默认/Master/空字符串 context 作为末尾参数正确传递，显式空 options 可与 context 同时使用，旧 (vm, {}) 调用继续有效；辅助窗角色与尺寸、空/非空弹窗标题独立生效，空字符串不回退默认视图，失败 Future/busy/VM 清理保持正常。根窗口同时验证 Master context 与显式尺寸。Core 的 dialogContextParameters 验证测试替身分别记录 context/options，以及下一次省略参数不会保留上次记录。

本次完整 Debug 构建、109 项 CTest 和 all_qmllint 均通过；仅框架构建/静态检查通过；重新复制到仓库外的独立消费工程构建、静态检查和 1 项 CTest 通过。最终构建日志没有编译警告或重复链接库警告。未重复执行 GUI 操作或其他平台验证，上文 GUI 记录仍为前一次 Multi-View 功能验收结果。
