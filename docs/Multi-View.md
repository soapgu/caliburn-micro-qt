# Multi-View：context 多视图定位

同一个 ViewModel 可以根据 context 选择不同的 View，也可以同时由多个 ViewHost 展示。本功能仅负责定位和装配，不提供 IViewAware、View 实例缓存或集合导航中的 View 保留。

## CM 3.2.0 对齐依据

参考本地 Caliburn.Micro 的 `3.2.0` 标签（提交 `544babd1865846af175ba32d86c839eb354714aa`）：

- [ViewLocator.TransformName](https://github.com/Caliburn-Micro/Caliburn.Micro/blob/3.2.0/src/Caliburn.Micro.Platform/ViewLocator.cs)：非 null context 移除 View 后缀，追加默认分隔符 `.` 和 context，只保留对应 context 候选，不回退默认 View。
- [View.OnContextChanged](https://github.com/Caliburn-Micro/Caliburn.Micro/blob/3.2.0/src/Caliburn.Micro.Platform/View.cs)：同一模型改变 context 后重新定位并绑定。
- [WPF WindowManager](https://github.com/Caliburn-Micro/Caliburn.Micro/blob/3.2.0/src/Caliburn.Micro.Platform/net40/WindowManager.cs)：窗口创建将 context 传给定位和绑定链路。

Qt 适配：类型名称对应部署后的 QML 资源路径；context 限定为可空字符串，不实现 CM 任意 object 的 ToString 语义。沿用现有搜索根与候选顺序，以及空 URL/errorString/Future 异常的错误契约，不创建 CM 的错误 TextBlock。context 分隔符固定采用 CM 默认 `.`，不新增规则配置入口。空字符串属于非默认 context，但无法形成约定视图名；允许通过显式登记为它提供 View。为避免路径越界，拒绝 `/`、`\` 和空命名段（如 `A..B`）。不去除空白，大小写保持原样。

CM LocateForModel 的 IViewAware 弱引用复用不属于本轮范围，Qt 宿主仍每次创建 View；不能将本轮描述为完整移植 CM 视图实例管理。

## 配置与搜索

```cpp
ViewLocator::addSearchRoot({QUrl("qrc:/qt/qml/MyApp/"), QStringLiteral("App")});
// 常规布局用约定；只有需要覆盖时显式登记。
ViewLocator::registerView<HomeViewModel>(defaultUrl);
ViewLocator::registerView<HomeViewModel>(masterUrl, QStringLiteral("Master"));
ViewLocator::registerView<HomeViewModel>(emptyContextUrl, QStringLiteral(""));
const auto defaultView = ViewLocator::viewUrl(model);
const auto masterView = ViewLocator::viewUrl(model, QStringLiteral("Master"));
```

`viewUrl` 与 `registerView<T>` 的 context 为 `const std::optional<QString>&`，默认 `std::nullopt`。显式键由实际 QMetaObject 与 context 组成，不沿基类查询；默认 context 和空字符串分开保存。同键同 URL 可重复登记，冲突拒绝并保留原值；freeze 同时冻结所有 context 的登记和搜索根配置。登记、配置、查询仍要求应用主线程。

以 `App::ViewModels::HomeViewModel`、根前缀 App 为例：

| context | Views 映射候选 | 同命名空间候选 |
| --- | --- | --- |
| 未指定 | views/HomeView.qml | ViewModels/HomeView.qml |
| Master | views/Home/Master.qml | ViewModels/Home/Master.qml |
| Detail | views/Home/Detail.qml | ViewModels/Home/Detail.qml |
| Detail.Compact | views/Home/Detail/Compact.qml | ViewModels/Home/Detail/Compact.qml |
| 空字符串 | 无约定候选 | 无约定候选 |

先查对应 context 的显式映射；没有映射时，先按根顺序检查全部 Views 映射候选，再检查全部同命名空间候选。应用根位于内置根之前，URL 去重后取首个存在的文件，qrc 和本地 file 根使用同一规则。不递归扫描，不缓存定位结果或 View 实例。qrc 资源名称严格区分大小写；本地 file 根的存在性检查沿用文件系统的大小写语义，候选名称本身不转换大小写。

指定 context 后不使用默认登记或默认约定 View。选中显式 URL 后，即使文件缺失也不回退约定；选中文件后，加载、根类型或 typed 注入失败不换其他候选。

## QML 与宿主

```qml
ViewHost {
    model: shell.home
    context: "Master"
}
ViewHost {
    model: shell.home
    context: "Detail"
}
```

`ViewLocator.resolve(model, context)` 的 QVariant 边界只接受字符串或空值。省略、null、undefined 表示默认 context；空字符串是独立键；数字、布尔、数组、对象、装箱字符串不自动转换，返回空 URL 并输出诊断。

ViewHost.context 为 var，默认 null；model/context 在组件完成前设置不会提前创建 View。完成后改变任一属性会卸载旧 View 并重新装配，赋相同 context 值不重建。两个宿主引用相同 VM 时各自持有独立 View；VM 数据共享，View 局部输入独立且切换重建后清空。宿主不调用 VM 激活/停用，也不改变 VM 身份。

Loader 继续负责 View 所有权；创建前注入 required typed viewModel，失败清空 item 并设置 errorString。受管 VM 的每个 View 都登记借用，关闭等待全部 View 释放后回收 VM；宿主销毁不删除借用模型，模型销毁则清空仍在使用它的宿主。定位与装配日志记录 context，`<default>` 与 `""` 可区分。

## 窗口与弹窗

```cpp
RootViewOptions rootOptions;
displayRootViewFor<ShellViewModel>(rootOptions, QStringLiteral("Desktop"));

WindowOptions windowOptions;
windowOptions.role = WindowRole::Auxiliary;
windows->showWindow(QVariant::fromValue(auxiliary), windowOptions, QStringLiteral("Monitor"));

DialogOptions dialogOptions;
dialogOptions.title = QStringLiteral("确认");
auto result = windows->showDialogAsync(std::move(dialogModel), requester,
                                       dialogOptions, QStringLiteral("Compact"));
```

context 是独立的可空字符串参数，追加到 options 之后；showDialogAsync 保留 requester 第二位。WindowOptions/ DialogOptions 只保存展示配置。普通窗口与根窗口按 context 定位窗口根；弹窗预检与内部 ViewHost 使用同一 context。省略 context/options 或只传 options 的旧业务调用均保持原用法，不需要插入 std::nullopt，不添加兼容重载。IWindowManager 的虚函数签名和派生覆盖仍须同步迁移并重新编译消费工程。参数顺序以 Qt 项目旧业务调用兼容为优先，不声称与 CM 的 model/context/settings 顺序一致。context 不放宽同 VM 的窗口关联限制，不改变角色、守卫、生命周期、弹窗 Future 和所有权契约；普通窗口根仍要求 Window/ApplicationWindow，Item 自动包装仍为后续待办。

窗口未找到指定 context 返回 false；弹窗以异常完成 Future 并清理资源，不回退默认 View。弹窗加载失败同样结束 busy 状态，后续可以继续请求。

## 示例与验收

[独立 multiview 示例](../examples/multiview/README.md) 使用 Shell 持有的唯一 HomeViewModel，同时展示 Master 与 Detail，并允许切换右侧 context。它是本项目自建最小演示，不是官方 HelloScreens 示例的移植。

实际检查结果见 [Multi-View 验收记录](验收/Multi-View验收记录.md)。Page 后缀、自定义转换规则、ViewAware、View 实例保留仍不在本次交付范围。

## 保持旧业务调用兼容

```cpp
windows->showWindow(QVariant::fromValue(model), windowOptions);
windows->showDialogAsync(std::move(dialogModel), requester, dialogOptions);
displayRootViewFor<ShellViewModel>(rootOptions);
```

showWindow(vm)、showDialogAsync(vm, requester) 和 displayRootViewFor<T>() 保持有效。C++ context 参数默认 std::nullopt，空字符串仍为独立 context；本次参数迁移不改变 ViewLocator 或 ViewHost 的行为。

只指定 context 时，用空 options 占位：

```cpp
windows->showWindow(QVariant::fromValue(model), {}, QStringLiteral("Master"));
windows->showDialogAsync(std::move(dialogModel), requester, {}, QStringLiteral("Compact"));
displayRootViewFor<ShellViewModel>({}, QStringLiteral("Desktop"));
```
