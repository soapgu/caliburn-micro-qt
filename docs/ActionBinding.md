# ActionBinding：操作、守卫与输入适配

> 状态：目标完整契约，尚未实现。本文所有代码均为设计示例，部分片段省略模块导入、完整类型注册及工程配置，不能直接作为可运行工程。当前没有任何批次已支持的能力。

ActionBinding 把两个问题连接起来：**这个操作现在能不能执行？触发后调用 VM 的哪个方法？**

它是计划中的自定义 C++ QObject 类型，可以由 QML 创建，没有视觉内容。它与负责展示的 [ViewHost](ViewHost.md) 分工配合；完整类型清单见 [README](../README.md)。

ActionBinding、ActionButton、KeyActionBinding 和 VM 基类统一属于计划中的 `Caliburn.Micro.Qt 1.0` 框架模块。用户模块定义具体操作、守卫和业务枚举，通过公开 C++ 头文件与 CMake 目标依赖框架，QML 显式导入框架模块。框架不依赖游戏类型或 DI 库；完整边界见 [模块与项目结构](模块与项目结构.md)。

按 [迭代实现计划](迭代实现计划.md)，第一批实现无参数 ActionBinding 与 ActionButton，用单页面按钮和文字绑定验证；第二批加入单个 int 参数及 KeyActionBinding。本文的参数、键盘和游戏示例属于完整目标，不是第一版已经支持的能力。

target 的共同类型契约见 [ViewModelBase](ViewModelBase.md)：基类提供类型锚点及 protected setAndNotify，不预置 canXxx、业务属性或信号。属性及 NOTIFY 由具体 VM 声明，ActionBinding 从实际目标的元对象读取，而不是要求基类提供统一属性变化事件。

## 1. 从普通按钮写法开始

不使用 ActionBinding，QML 可以直接连接方法和条件：

```qml
// 设计示例：直接绑定应用 VM，省略模块导入。
Button {
    text: "暂停"
    enabled: viewModel.canPauseGame
    onClicked: viewModel.pauseGame()
}
```

这是一种正常、清楚的写法。按钮少时并不需要额外抽象。

操作入口增多后，按钮、菜单和快捷键需要重复遵守“方法与守卫匹配”的规则。ActionBinding 把验证、守卫连接与执行前复核集中起来；ActionButton 则包装按钮的通用接线：

```qml
// 设计示例：使用通用按钮包装。
ActionButton {
    text: "暂停"
    target: viewModel
    action: "pauseGame"
}
```

`action` 是方法名配置。声明它不会执行方法；用户触发后才调用绑定的 execute()。

## 2. 与 WPF ICommand、CM ActionMessage 的对应关系

| 含义 | WPF / Caliburn.Micro | 当前 Qt 设计 |
| --- | --- | --- |
| 操作目标 | DataContext / Action.Target 等 | 显式 target。 |
| 执行方法 | ICommand.Execute / ActionMessage 方法调用 | VM 的公开 Q_INVOKABLE 方法。 |
| 可执行条件 | CanExecute / CanXxx 守卫 | 布尔 canXxx 属性。 |
| 条件变化通知 | CanExecuteChanged / PropertyChanged | Qt NOTIFY 信号。 |
| 参数 | CommandParameter / Action 参数 | arguments 参数列表。 |

这更接近 CM 的“方法 + CanXxx”约定，不是把 ICommand 接口直接移植到 Qt。CM 还支持寻找动作目标、守卫方法、复杂参数等能力；本轮目标采用显式 target 和有限参数契约，不沿视觉树冒泡寻找 VM。

## 3. 接口与操作契约

| 成员 | 目标类型与读写 | 含义 |
| --- | --- | --- |
| target | ViewModelBase*，可写，带 NOTIFY | 借用接收操作的 VM。 |
| action | QString，可写，带 NOTIFY | 方法名，如 pauseGame。 |
| arguments | QVariantList，可写，带 NOTIFY | 第一批只接受空列表；第二批加入单个 int 参数列表。 |
| enabled | bool，只读，带 NOTIFY | 目标、配置有效且当前守卫允许执行。 |
| execute() | Q_INVOKABLE void | 重新检查后请求执行操作。 |

目标支持的方法形式（分批实现）：

```cpp
// 设计示例：合法的目标方法形态。
Q_INVOKABLE void pauseGame();                      // 第一批实现此形态。
Q_INVOKABLE void selectDifficulty(int difficulty); // 第二批加入此形态。
```

必须是公开 Q_INVOKABLE void 方法；无参数或单个 int，不支持重载、返回值、多参数或传控件。参数数量和类型必须匹配，不能把任意字符串或任意对象当成可执行参数。

第一批非空 arguments 或带参数方法应诊断并禁用，不临时忽略参数，也不以占位实现声称支持单个 int。第一批 ShellViewModel 直接继承 ViewModelBase，ShellView 为唯一页面，按钮目标指向 Shell；第三批计数操作整体移入 Home，Shell 根窗口通过 ViewHost 展示它。下文 GameViewModel 继承 ScreenViewModel 的片段属于后续完整目标，第一批不依赖 Screen。

守卫名为 `can` 加动作名首字母大写：

| action | 对应属性 |
| --- | --- |
| pauseGame | canPauseGame |
| resumeGame | canResumeGame |
| selectDifficulty | canSelectDifficulty |
| turn | canTurn |

守卫必须是可读 bool Q_PROPERTY，具有 NOTIFY。普通 C++ bool getter 若没有属性声明，不符合这个契约。只供 C++ 使用的生命周期、工厂及应用方法也不会自动成为可绑定操作。

## 4. 独立使用 ActionBinding

```qml
// 设计示例：页面中的绑定与按钮，省略外围 Item 和模块导入。
ActionBinding {
    id: pauseBinding
    target: viewModel
    action: "pauseGame"
    arguments: []
}

Button {
    text: "暂停"
    enabled: pauseBinding.enabled
    onClicked: pauseBinding.execute()
}
```

这是声明“调用目标的 pauseGame，并用对应守卫控制按钮”。一个页面可以有多个绑定，各入口也可以各自创建绑定并指向同一操作，不要求共用同一个绑定实例。

## 5. VM 提供方法、守卫与通知

```cpp
// 设计示例：应用 GameViewModel 的接口片段，非完整类实现。
class GameViewModel : public ScreenViewModel
{
    Q_OBJECT
    Q_PROPERTY(bool canPauseGame
               READ canPauseGame
               NOTIFY canPauseGameChanged)

public:
    bool canPauseGame() const;
    Q_INVOKABLE void pauseGame();

signals:
    void canPauseGameChanged();
};
```

守卫可以综合页面、交互和业务状态。以下辅助方法仅表达条件，并非框架接口：

```cpp
// 设计示例：应用内部守卫计算。
bool GameViewModel::canPauseGame() const
{
    return isActive()
        && !interactionPending()
        && !dialogsBusy()
        && sessionIsRunning();
}
```

ActionBinding 不知道“暂停需要什么业务条件”，也不会观察会话的全部属性来推导规则。应用 VM 订阅相关变化、重新计算守卫，并在结果变化时发出通知。

VM 可以使用 ViewModelBase 的 `setAndNotify` 比较、赋值并发送指定无参数信号；这不会自动通知依赖该值的独立 canXxx 守卫。第一批 Shell 的 count/message 共用 countChanged，两个守卫由 Shell 另外比较并通知，完整示例见 [ViewModelBase](ViewModelBase.md)。第三批迁入 Home 后保持这些通知与操作约定，第五批 Home 重置和 Detail 离开再加入确认。

```text
弹窗或会话状态变化
    → VM 重新计算 canPauseGame
    → 结果变化，发出 canPauseGameChanged
    → ActionBinding 读取新值，更新 enabled
    → enabledChanged 通知 QML
    → Button.enabled 刷新
```

| 应用场景 | canPauseGame 示例结果 |
| --- | --- |
| 活动页面运行中，无弹窗或待确认操作 | true |
| 已暂停或已结算 | false |
| 正在处理确认操作 | false |
| 页面已经停用 | false |

## 6. 执行时为什么还要检查

enabled 表达最近计算的展示状态，实际执行发生在另一个时间点。状态可能在点击前变化，也可能由快捷键或代码直接调用 execute()。

目标执行流程：

```text
execute()
    → 检查目标仍存在，且方法、参数与守卫配置有效
    → 重新读取 canXxx
    → false：结束，不调用
    → true：在同一 GUI 线程调用目标方法
```

调用方法后，VM 仍应检查操作前置条件，因为 C++ 调用者可能绕过绑定。业务用例再检查最终业务状态。

| 层次 | 责任 |
| --- | --- |
| ActionBinding | 统一入口的配置检查、守卫检查与调用。 |
| VM | 操作条件、交互锁、确认流程及意图编排。 |
| Model / 服务用例 | 最终业务状态与规则。 |

execute() 不返回 View，也不返回同步弹窗结果。需要确认的操作由 VM 请求弹窗服务，在有效的异步结果到达后调用用例。

## 7. ActionButton 包装通用接线

```qml
// 设计示例：ActionButton.qml，省略框架模块导入。
import QtQuick
import QtQuick.Controls

Button {
    property alias target: binding.target
    property alias action: binding.action
    property alias arguments: binding.arguments

    enabled: binding.enabled
    onClicked: binding.execute()

    ActionBinding {
        id: binding
    }
}
```

页面使用时只配置文字、目标和动作：

```qml
// 设计示例：省略外围布局与模块导入。
ActionButton {
    text: "暂停"
    target: viewModel
    action: "pauseGame"
}

ActionButton {
    text: "继续"
    target: viewModel
    action: "resumeGame"
}
```

通用组件内部仍有 onClicked。被集中的是接线和检查，每个业务页面不再重复判断状态。按钮保留 Qt Quick Button 原生的空格触发行为。

## 8. 单个 int 参数与守卫边界

```cpp
// 设计示例：DifficultyViewModel 的接口片段。
Q_PROPERTY(bool canSelectDifficulty
           READ canSelectDifficulty
           NOTIFY canSelectDifficultyChanged)

public:
    bool canSelectDifficulty() const;
    Q_INVOKABLE void selectDifficulty(int difficulty);

signals:
    void canSelectDifficultyChanged();
```

```qml
// 设计示例：GameEnums 是应用提供的枚举类型，不属于框架。
// 此片段位于 QtSnakeLab 用户模块的页面中，类型均待实现。
import QtQuick
import Caliburn.Micro.Qt 1.0
import QtSnakeLab 1.0

ActionButton {
    text: "简单"
    target: viewModel
    action: "selectDifficulty"
    arguments: [GameEnums.Difficulty.Easy]
}
```

调用相当于向 selectDifficulty(int) 传入对应枚举值。操作绑定检查参数形态，VM 检查是否为合法领域枚举，用例复核当前是否允许变更。

目标 canSelectDifficulty 不接收参数，回答的是“现在是否允许修改难度”。它不会根据每个按钮的难度值分别计算。如果未来需要某一档单独禁用，需要扩展契约或增加专门展示属性，本轮目标不包含参数化守卫。

## 9. KeyActionBinding 如何复用操作

KeyActionBinding 是视觉输入适配器，目标属性为 `Item host`、`ViewModelBase target`、`string action`、`var keyMap`。keyMap 将 Qt 键码映射到参数列表。

```qml
// 设计示例：键盘绑定，boardView 为应用中可接收按键的 Item。
KeyActionBinding {
    host: boardView
    target: viewModel
    action: "turn"

    keyMap: ({
        [Qt.Key_Up]: [GameEnums.Direction.Up],
        [Qt.Key_W]:  [GameEnums.Direction.Up],
        [Qt.Key_Down]: [GameEnums.Direction.Down],
        [Qt.Key_S]:    [GameEnums.Direction.Down]
    })
}
```

向上键和 W 都变成 turn(int) 的方向参数。VM 不接收键盘事件，也不需要知道方向来自哪个物理键。

```text
按钮点击 / 页面按键
    → 各自的输入适配
    → ActionBinding 的配置、参数和守卫检查
    → 同一个 VM 操作
```

输入契约：

- 仅在 host 的活动焦点路径内处理，按 Keys.AfterItem 的顺序让控件先处理。
- 忽略自动重复与已消费事件；识别到配置键且操作可用后执行并消费。
- 无参数操作映射为空列表；方向操作映射为单个 int 列表。
- 按钮已经消费的空格不再触发页面的暂停；弹窗消费的 Esc 不穿透底层页面。
- 键盘事件留在视觉适配层，不向 VM 传 QKeyEvent 或控件。

这不是全局快捷键管理器。焦点与模态隔离属于视觉层，canXxx 负责操作条件，两者需要配合。

## 10. 元对象检查与配置错误

ActionBinding 使用 Qt 元对象系统找到方法、属性与通知信号，而不是通过脚本 eval 执行任意表达式。

绑定建立或 target/action/arguments 更新时，目标流程是：

1. 断开旧守卫和旧目标连接，清除旧方法配置。
2. 检查新目标与 action，查找公开 Q_INVOKABLE void 方法并排除重载。
3. 检查方法参数数量和类型，匹配 arguments。
4. 查找对应可读 bool canXxx 属性及 NOTIFY。
5. 连接通知、读取守卫并计算 enabled。

缺少方法、缺少守卫、参数不符或方法形态不支持时，输出开发诊断并禁用；null 目标作为未装配状态禁用。字符串拼错属于绑定时才能发现的配置错误，未来测试需要覆盖这些情况。

同线程是展示绑定的前提。本轮目标不提供跨线程调用或排队执行语义，避免检查完守卫后再跨线程排队造成额外时序差异。元方法调用失败也应明确诊断。

## 11. 目标销毁、替换与动态页面

ActionBinding 只借用目标，不拥有它。内部弱目标可使用 QPointer<ViewModelBase>：QObject 销毁后指针自动变空，但绑定仍需处理销毁通知、清理连接并发出必要的 targetChanged/enabledChanged。

```cpp
// 设计示例：弱目标成员，非完整实现。
QPointer<ViewModelBase> m_target;
```

QPointer 不延长目标寿命，也不自动取消应用的异步确认回调。回调有效性由弹窗服务的弱请求者与请求标识负责。

动态返回页面时：

```text
返回用例成功
    → VM 停用，守卫拒绝新操作，并取消所属弹窗请求
    → 活动页切换，旧 View 与其绑定卸载
    → 应用清空访问指针，安排旧 VM.deleteLater()
    → VM 实际销毁，残留弱绑定目标变空并禁用
```

延迟删除之前靠停用与守卫阻止操作，实际删除之后靠弱引用阻止调用。重新进入创建的新 VM 必须建立新绑定，旧对象的通知不能更新它的按钮。

## 12. 待实现验收场景

以下为目标完整验收清单；第一批验证无参数绑定与通知、守卫及目标寿命，第二批增加参数/键盘检查，导航与确认场景随第四、五批加入。目前全部未验证。

- 第一批 ShellViewModel 继承框架基类，ShellView 注入 typed VM，ActionBinding 接收跨模块目标并识别其操作与守卫；静态插件和类型注册正确保留。第三批迁入 Home 后回归同样的绑定行为。
- 方法、返回类型、参数数量/类型、重载以及 bool 守卫/NOTIFY 验证。
- 守卫变化自动刷新 enabled；直接 execute() 仍重新读取守卫。
- null 或销毁目标禁用，不延长 VM 生命周期。
- 更换目标、动作或参数后断开旧连接，旧通知不污染新状态。
- 按钮与快捷键调用同一 VM 操作，参数合法性仍由 VM/用例检查。
- 活动焦点、自动重复、空格重复消费与模态 Esc 穿透检查。
- 动态页面卸载、延迟释放和旧确认回调失效。

这些是未来验收设计，不是已有通过结果。

## 参考资料

- [Caliburn.Micro：操作、目标与守卫](https://caliburnmicro.com/documentation/actions)
- [Qt：向 QML 暴露 C++ 属性、方法与通知](https://doc.qt.io/qt-6.8/qtqml-cppintegration-exposecppattributes.html)
- [Qt QMetaObject：方法与属性元信息](https://doc.qt.io/qt-6.8/qmetaobject.html)
- [Qt QMetaMethod：方法调用](https://doc.qt.io/qt-6.8/qmetamethod.html)
- [Qt QPointer：QObject 弱引用](https://doc.qt.io/qt-6.8/qpointer.html)
- [Qt Keys：焦点路径与事件处理顺序](https://doc.qt.io/qt-6.8/qml-qtquick-keys.html)
- [Qt Button](https://doc.qt.io/qt-6.8/qml-qtquick-controls-button.html)
