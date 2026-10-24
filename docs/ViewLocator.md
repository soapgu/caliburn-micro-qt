# ViewLocator：视图约定定位

第七批统一了视图的登记、配置、冻结及查询入口。公开 C++ 头文件为 `CaliburnMicroQt/ViewLocator.h`，QML 导入 `Caliburn.Micro.Qt 1.0` 后使用 `ViewLocator.resolve(model)`。`ViewRegistry` 已移入框架内部，不再提供公开头文件或 QML 类型，也没有兼容转发接口。

## 配置与查询

```cpp
#include <CaliburnMicroQt/ViewLocator.h>

// 在 configure 中配置一次；以部署后的资源路径为准。
const bool configured = ViewLocator::addSearchRoot({
    QUrl(QStringLiteral("qrc:/qt/qml/MyApp/")),
    QStringLiteral("MyApp"),
    QStringLiteral("views")
});
// 仅为不符合约定或需要覆盖默认视图的类型显式登记。
const bool registered = ViewLocator::registerView<SpecialViewModel>(
    QUrl(QStringLiteral("qrc:/qt/qml/MyApp/special/Editor.qml")));
// BootstrapperBase 在 configure 成功后自动冻结；独立使用时自行调用。
const bool frozen = ViewLocator::freeze();
const QUrl url = ViewLocator::viewUrl(model);
```

`ViewSearchRoot` 包含 `QUrl url`、`QString modelNamespacePrefix` 和默认值为 `views` 的 `QString viewsDirectory`。`registerView<T>(url)`、`addSearchRoot(root)`、`freeze()` 返回 bool。`viewUrl(const QObject*, const std::optional<QString>& context = std::nullopt)` 返回 QUrl；每引擎查询单例提供 `Q_INVOKABLE QUrl resolve(QObject*, const QVariant& context = {}) const`。`registerView<T>(url, context)` 同样接受可空 context。登记、配置及冻结只向 C++ 提供。

登记类型必须继承 QObject，并声明自己的 Q_OBJECT；模板在编译期检查，不能借用父类的类型身份。定位和 ViewHost 接受直接继承 QObject 的模型，属性刷新依赖模型自己的 Q_PROPERTY / NOTIFY。ViewModelBase 仍是提供 setAndNotify 的便利基类。WindowManager、Bootstrapper、Conductor、Parent 和关闭策略仍保持原有类型、生命周期和所有权约束；可定位的普通 QObject 不自动获得受管窗口或 Conductor 支持。

所有配置和查询均限定应用主线程，模型也必须属于该线程。配置为进程级共享，每个 QML 引擎拥有独立 Locator 单例；引擎销毁不清除配置，Locator 不持有模型。查询不会自动冻结。freeze 同时冻结登记及搜索根，重复冻结成功，冻结后的所有配置操作均拒绝。本阶段不提供解冻、清空或运行期重配置接口。

## 定位优先级

1. 显式映射，按实际 QMetaObject 与可空 context 精确匹配。
2. 约定搜索，按实际类型名称生成候选，不沿基类回退；应用与框架使用相同规则。

显式登记接受有效、非空、绝对 URL，不提前检查资源存在性。冻结前同类型、同 context、同 URL 重复登记成功，同类型同 context 的不同 URL 冲突拒绝，原映射保留。Registry 只保存显式映射，框架不预先登记确认视图。应用可显式覆盖任何可登记类型；显式地址缺失或加载失败时，不回退约定候选。未知类型、空模型和非法线程查询返回空 URL，非法操作输出 view 分类诊断。

## 搜索根与候选

根支持 `qrc:/` 和绝对本地 `file:///` 目录，自动补齐末尾 `/`。拒绝相对 URL、其他协议、查询串、片段及远程 authority；根目录不必预先存在。完全相同的规范化根配置重复添加成功。`viewsDirectory` 允许 `views`、`Views` 等单个非空目录名，拒绝 `.`、`..` 和路径分隔符。

Locator 初始化时自动包含 `qrc:/qt/qml/Caliburn/Micro/Qt/` 搜索根，前缀为空、目录为 `views`；独立使用也有效，不依赖 Bootstrapper。应用添加的根按添加顺序插入内置根之前，内置根始终保留在最后。所有根共用候选生成、去重和文件存在性检查，不按模型类型提供特判或默认映射表。

`modelNamespacePrefix` 按完整 `::` 分段匹配并移除，空前缀不限类型范围。例如 `App` 匹配 `App::Feature`，不匹配 `Apple`。剩余命名空间转换为目录，保留大小写；类型末尾 `ViewModel` 转换为 `View.qml`，基本名称为空或后缀不匹配时不生成候选。

下表以根 `qrc:/qt/qml/App/`、前缀 `App`、目录 `views` 为例：

| 类型 | Views 映射候选 | 同命名空间候选 |
| --- | --- | --- |
| App::HomeViewModel | views/HomeView.qml | HomeView.qml |
| App::ViewModels::HomeViewModel | views/HomeView.qml | ViewModels/HomeView.qml |
| App::ViewModels::Feature::DetailViewModel | views/Feature/DetailView.qml | ViewModels/Feature/DetailView.qml |
| App::Feature::HomeViewModel | 无 | Feature/HomeView.qml |
| App::ViewModels::ViewModels::HomeViewModel | ViewModels/views/HomeView.qml | ViewModels/ViewModels/HomeView.qml |

仅替换最后一个完整 `ViewModels` 段。所有匹配根先检查 Views 映射候选，再检查同命名空间候选；每组按根配置顺序检查，应用根在前、框架根最后，候选去重后取首个存在的文件。空前缀、无命名空间的示例同样依次生成 `views/HomeView.qml` 和 `HomeView.qml`。使用资源别名生成实际部署 URL，不推断源码目录，不递归扫描资源，也不增加定位缓存。

例如配置空前缀应用根 `qrc:/qt/qml/MyApp/` 后，ConfirmActionViewModel 依次查询：

1. `qrc:/qt/qml/MyApp/views/ConfirmActionView.qml`
2. `qrc:/qt/qml/Caliburn/Micro/Qt/views/ConfirmActionView.qml`
3. `qrc:/qt/qml/MyApp/ConfirmActionView.qml`
4. `qrc:/qt/qml/Caliburn/Micro/Qt/ConfirmActionView.qml`

内置视图保持第 4 个资源地址。应用在第 1 个地址部署 typed ConfirmActionViewModel 的 QML 即可覆盖，无须登记；显式 `registerView<ConfirmActionViewModel>(url)` 则优先于以上所有候选。ConfirmActionViewModel 位于全局命名空间；限定 `MyApp` 前缀的根不匹配它，覆盖时须另加空前缀根。

定位只检查文件存在性，不试创建 View。选定文件后，加载、根类型或 typed 注入失败即报告，不继续换候选。根窗口、子页面和弹窗统一经过 Locator，创建与注入仍由 WindowManager 和 [ViewHost](ViewHost.md) 完成。view 分类 Debug 日志记录根配置、模型类型、搜索范围、候选及命中结果，失败输出 Warning。

## 调用迁移与后续范围

旧 C++ `ViewRegistry::registerView/freeze/viewUrl` 迁移为 `ViewLocator` 同名入口，QML `ViewRegistry.resolve` 迁移为 `ViewLocator.resolve`。常规页面逐项登记改为一次 addSearchRoot；资源声明、QML 模型类型注册和 C++ 构造注入仍须由应用完成。示例 Shell、Home、Detail 已采用模块根 `qrc:/qt/qml/CaliburnExample/`。

context 多视图的规则、参数边界与示例见 [Multi-View](Multi-View.md)。未传 context 时继续使用上文的默认视图规则；指定 context 后只查对应映射与约定候选，不回退默认 View。

`Page` 后缀、自定义转换规则见 [后续扩展](计划/后续版本ToDoList.md#视图定位约定的后续扩展待设计待实施)。窗口管理及 Conductor 的进一步接口化、Action 绑定和 View 实例保留均为独立待办，本批不提供相关实现或占位接口。

本机验证结果与 Cocoa 首轮焦点超时及前台复验记录见 [第七批验收记录](验收/第七批验收记录.md)。
