# ViewModelBase：类型基础与通知辅助

> 状态：第一批已实现本文基类与类型化通知辅助，macOS arm64 / Qt 6.8.3 验收通过，见 [记录](第一批验收记录.md)。下文接口片段省略模板定义，完整定义位于框架公开头文件；Screen/Conductor 仍待后续批次实现。

ViewModelBase 是所有框架与业务 VM 的共同 QObject 基类，提供两项能力：**作为框架接口的类型锚点，以及辅助派生 VM 比较、赋值并发送通知**。

它不增加 displayName、业务属性或生命周期状态。第一批已实现该基类与类型化通知辅助，用 ShellViewModel 的单页面按钮和文字绑定验证；Screen 生命周期在第三批加入。模块归属见 [模块与项目结构](模块与项目结构.md)，交付顺序见 [迭代实现计划](迭代实现计划.md)。

## 1. 类型锚点有什么作用

| 使用位置 | ViewModelBase 的作用 |
| --- | --- |
| `ViewHost.model` | 接收 ViewModelBase 派生对象，视图注册表根据实际 VM 类型定位 View。 |
| `ScreenViewModel` | 在共同类型和通知辅助之上增加初始化、激活、停用；不把生命周期放回基类。 |
| 用户 `ShellViewModel`、后续 `HomeViewModel` 等 | 继承框架共同类型，自己声明属性、信号、操作与业务依赖。 |

框架可以接收用户模块的具体 VM，但不需要包含用户 VM 头文件。普通 QObject 不是本项目的 VM 类型，不能直接作为这些类型化入口的替代对象。框架读取的是实际派生对象的元对象，不因使用基类指针而丢失派生类属性。

## 2. 完整成员契约

下面列出已实现基类的完整成员接口；模板定义位于 [公开头文件](../modules/Caliburn/Micro/Qt/include/CaliburnMicroQt/ViewModelBase.h)，构造函数位于框架源码。

```cpp
// 已实现接口：此处省略模板定义，完整源码见公开头文件。
#include <QObject>
#include <QtQmlIntegration/qqmlintegration.h>

class ViewModelBase : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("ViewModel 由 C++ 应用装配层创建")

public:
    explicit ViewModelBase(QObject *parent = nullptr);
    ~ViewModelBase() override = default;

protected:
    template<class Owner, class T>
    bool setAndNotify(
        T &field,
        const T &value,
        void (Owner::*notifySignal)());
};
```

| 新增成员或声明 | 契约 |
| --- | --- |
| 构造函数 | 只将 parent 交给 QObject，不创建服务、子 VM 或 View，不设置 QML 所有权。 |
| 默认虚析构 | 沿用 QObject 对象销毁行为，无关闭用例、停用钩子或额外资源处理。 |
| protected `setAndNotify` | 供派生类 C++ 方法使用，不暴露成 QML 操作。 |
| `Q_OBJECT` | 提供本类元对象，支持框架类型识别及派生层的 Qt 协作。 |
| `QML_ELEMENT`、`QML_UNCREATABLE` | 以 ViewModelBase 名称注册到 `Caliburn.Micro.Qt 1.0`；QML 可以声明类型化属性，不能创建实例。 |

基类**不新增数据成员、Q_PROPERTY、信号、槽或 Q_INVOKABLE 方法**。它不是纯虚抽象类，C++ 可以构造它；不可由 QML 创建是注册规则，不是 C++ 抽象性。[Qt QML 注册宏说明](https://doc.qt.io/qt-6.8/qqmlintegration-h.html#QML_UNCREATABLE)

QObject 的继承成员继续存在，例如 objectName、destroyed、parent、deleteLater；“不新增属性/信号”不表示这些继承能力消失。objectName 仅可用于对象识别或调试，不承担页面标题或视图映射职责。复制和移动沿用 QObject 的限制，不提供克隆或值对象语义。[Qt QObject 说明](https://doc.qt.io/qt-6.8/qobject.html)

框架公开头文件路径为 `include/CaliburnMicroQt/ViewModelBase.h`；构造函数实现放入框架源码，模板定义随公开头文件提供，使用 C++17，不引入用户模块或 DI 库。

## 3. setAndNotify 的输入与执行结果

典型调用：

```cpp
// 设计示例：ShellViewModel 自己的字段与无参数 NOTIFY 信号。
setAndNotify(m_count, value, &ShellViewModel::countChanged);
```

### 参数约束

- `field` 是当前对象拥有的可写存储字段，`value` 与其类型一致；`T` 必须支持返回可判断结果的相等比较及复制赋值。第一批使用 int，接口也适用于满足条件的 QString 等值类型。
- `Owner` 必须是 ViewModelBase 或其派生类，并具备自己的 Q_OBJECT 声明；当前对象必须是 Owner 或其派生对象。
- `notifySignal` 只接受无参数、返回 void 的非 const 成员函数指针；调用者必须传正确的 NOTIFY 信号。带参数通知不在此辅助函数的支持范围内。
- 在 GUI 线程调用，当前对象与字段在调用及同步通知期间必须保持有效；辅助函数不转移所有权，也不负责异步任务。

实现使用编译期约束检查 Owner 的继承关系，运行时通过 `qobject_cast<Owner*>(this)` 确认实际对象兼容，再调用信号指针，不能未经检查就将 this 静态转成任意 Owner。Qt 对 qobject_cast 的类型及 Q_OBJECT 要求见 [官方说明](https://doc.qt.io/qt-6.8/qobject.html#qobject_cast)。

### 操作顺序

```text
检查信号指针非空、实际对象兼容 Owner
    → 非法：诊断并返回 false，不修改字段
    → 合法：比较 field 与 value
        → 相等：返回 false，不赋值、不通知
        → 不同：赋值，再同步调用指定信号，返回 true
```

| 场景 | 返回值 | 字段及通知 |
| --- | --- | --- |
| 信号为空或 Owner 与当前对象不兼容 | false | 开发诊断；字段不变，不通知。 |
| 合法参数且值相同 | false | 正常无变化；不诊断，不赋值、不通知。 |
| 合法参数且值不同 | true | 本次已更新字段，并调用一次指定信号。 |

信号观察者在正常更新路径中读取到的是已经提交的新值。辅助函数不使用排队调用；信号接收方仍遵循 Qt 连接语义，本项目的展示协作限定在同一 GUI 线程。同步信号期间调用者不能删除当前对象。

返回值表示本次字段是否发生更新，不表示业务命令是否被接受，也不作为 QML 操作的返回结果。业务验证在调用前完成；比较或赋值的异常处理沿用值类型自身行为，辅助函数不提供通用事务或回滚。

### 类型化能检查什么

成员指针让 C++ 检查方法名与签名，运行时的 Owner 检查防止在错误对象上调用。**普通 void 方法也可能具有相同签名**，因此它不证明传入方法一定是信号，更不证明它是 field 对应属性的 NOTIFY。

调用者保证字段、属性与信号匹配。辅助函数不通过反射验证这层对应关系，不按属性名找信号，不扫描依赖，也不自动通知其他属性。直接声明并发送信号仍然是合法方式，不强制所有通知都经过辅助函数。

## 4. 与 WPF 通知方式的区别

这里借鉴属性变化通知的职责，但不提供 WPF 风格的 `notifyPropertyChanged("count")` 字符串入口。

Qt 的声明把属性与特定通知信号关联起来，例如：

```cpp
Q_PROPERTY(int count READ count NOTIFY countChanged)
```

QML 依赖此处的 countChanged 更新绑定。增加一个普通的 `propertyChanged(QString)` 信号，不能替代 count 的 NOTIFY 声明。setAndNotify 只减少“比较旧值、赋值、通知”的重复代码，属性、读取方法和信号仍由派生 VM 声明。[Qt 属性系统](https://doc.qt.io/qt-6.8/properties.html)

多个计算属性可以明确复用同一个无参数 NOTIFY；它们在该信号发出时重新读取。不提供自动推导的属性依赖图，独立守卫的通知仍由 VM 根据实际变化发送。

## 5. 第一批 ShellViewModel 示例

下面片段表达已实现的第一批根 Shell VM 属性与通知路径；实际源码将声明与方法定义分开，见 [ShellViewModel.h](../examples/minimal/CaliburnExample/viewmodels/ShellViewModel.h) 和 [ShellViewModel.cpp](../examples/minimal/CaliburnExample/viewmodels/ShellViewModel.cpp)。Shell 是入口命名约定，第一批只继承 ViewModelBase；第三批才增加 Screen 生命周期并把计数、文案、参数操作及守卫整体移入 Home，第四批 Shell 再演进为 Conductor。

```cpp
// 第一批源码的等价展示：合并声明与方法定义，便于阅读。
#include <CaliburnMicroQt/ViewModelBase.h>
#include <QString>

class ShellViewModel : public ViewModelBase
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("由示例应用装配层创建")
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QString message READ message NOTIFY countChanged)
    Q_PROPERTY(QString incrementText READ incrementText CONSTANT)
    Q_PROPERTY(bool canIncrement READ canIncrement NOTIFY canIncrementChanged)
    Q_PROPERTY(bool canReset READ canReset NOTIFY canResetChanged)

public:
    explicit ShellViewModel(QObject *parent = nullptr)
        : ViewModelBase(parent) {}

    int count() const { return m_count; }
    QString message() const
    {
        return QStringLiteral("已点击 %1 次").arg(m_count);
    }
    QString incrementText() const { return QStringLiteral("增加"); }
    bool canIncrement() const { return m_count < 5; }
    bool canReset() const { return m_count > 0; }

    Q_INVOKABLE void increment()
    {
        if (canIncrement())
            updateCount(m_count + 1);
    }

    Q_INVOKABLE void reset()
    {
        if (canReset())
            updateCount(0);
    }

signals:
    void countChanged();
    void canIncrementChanged();
    void canResetChanged();

private:
    void updateCount(int value)
    {
        const bool oldCanIncrement = canIncrement();
        const bool oldCanReset = canReset();

        if (!setAndNotify(m_count, value, &ShellViewModel::countChanged))
            return;

        if (oldCanIncrement != canIncrement())
            emit canIncrementChanged();
        if (oldCanReset != canReset())
            emit canResetChanged();
    }

    int m_count = 0;
};
```

示例中 count 是唯一计数存储，message 和两个守卫均由它计算；调用 countChanged 时读取这些属性已经得到新值。message 复用 countChanged，不另发 messageChanged；两个守卫有各自的 NOTIFY，仅 bool 结果变化才通知。increment/reset 自身检查条件，直接从 QML 或 C++ 调用也不能越过计数范围。

这里假定属性观察者只读取展示状态，不在 countChanged 的同步处理栈中再次修改计数；辅助函数不串行化重入操作。后续业务服务状态的事务、依赖更新或操作重入由业务层自行处理，不能把 setAndNotify 当成批量通知框架。

与页面的关系是：ShellView 绑定 message 和 incrementText；按钮的 enabled 显式读取 canIncrement/canReset，onClicked 直接调用 increment/reset，见 [操作与输入绑定](操作与输入绑定.md)。这些名称属于示例 VM，不是框架约定。ViewModelBase 不持有按钮或窗口。

## 6. 所有权、线程与生命周期

| 边界 | 约定 |
| --- | --- |
| 根 VM | C++ 装配层创建，无 QObject 父对象，由 unique_ptr 等 RAII 所有者持有。 |
| 子 VM | 接管后由 QObject 父树唯一管理；不与 unique_ptr 同时负责删除。 |
| QML 引用 | 借用 C++ VM；应用在暴露前明确设置 CppOwnership，不由基类构造函数设置。 |
| 线程 | 本项目展示 VM 在 GUI 线程协作，通知辅助同线程同步调用，不提供工作线程调度能力。 |
| Screen 生命周期 | initialize/activate/deactivate 及状态通知属于 ScreenViewModel；基类构造/析构不能代替业务开始、结束或页面激活。 |

QML 不可创建类型与 CppOwnership 是不同契约：注册宏控制能否实例化，所有权设置控制引擎是否管理对象删除。应用可使用 `QQmlEngine::setObjectOwnership(vm, QQmlEngine::CppOwnership)`；这是引擎继承的所有权接口。[Qt 对象所有权说明](https://doc.qt.io/qt-6.8/qjsengine.html#ObjectOwnership-enum)

基类不保存 View、QML 引擎、容器、服务定位器或业务依赖。派生 VM 可借用构造注入的业务服务，服务寿命由应用装配保证；这不成为基类统一服务入口。关闭程序先销毁 View 与引擎，再销毁 VM 树及其借用服务，保持 [ViewHost](ViewHost.md) 的所有权约定。

## 7. 第一批验收场景

| 场景 | 预期结果 |
| --- | --- |
| 构造与元对象 | 构造保持指定 parent；基类和用户派生类类型可识别，不新增 displayName 等属性。 |
| 合法更新 | int/QString 等值类型更新，返回 true，指定无参数信号一次，观察者读取到新值。 |
| 同值更新 | 返回 false，无通知、无多余赋值；以测试值类型记录赋值次数。 |
| 空信号指针 | 使用有明确成员指针类型的空值，返回 false，诊断且字段不变。 |
| Owner 不兼容 | 传另一个 VM 类型的无参数信号，返回 false，诊断且字段不变。 |
| 签名约束 | 带参数或非 void 成员不匹配接口；Owner 继承关系纳入编译期约束。 |
| 派生通知 | Shell 的 count/message 刷新一致，独立守卫只在 bool 结果变化时通知。 |
| 跨模块与 QML | ShellViewModel 继承框架基类，ShellView 的 required property ShellViewModel viewModel 识别初始属性；直接创建 ViewModelBase 或 ShellViewModel 被拒绝，C++ 注入成功。 |
| 所有权 | QML 不接管 C++ VM；根 RAII 和子对象父树释放不重复删除，保持既定退出顺序。 |
| example | 点击到 5 禁用增加，重置归零，文字与按钮状态自动刷新；VM 直接调用仍检查条件。 |

上述场景已由第一批 C++ 契约测试、QML 集成测试及真实 example 操作验证；编译期断言检查辅助签名和 QObject 复制/移动限制。环境与结果见 [第一批验收记录](第一批验收记录.md)，麒麟仍待验证。

## 参考资料

- [Qt 属性系统：Q_PROPERTY、NOTIFY 与 CONSTANT](https://doc.qt.io/qt-6.8/properties.html)
- [Qt QObject：元对象、所有权与 qobject_cast](https://doc.qt.io/qt-6.8/qobject.html)
- [Qt QML 类型注册宏](https://doc.qt.io/qt-6.8/qqmlintegration-h.html)
- [Qt C++ 与脚本对象所有权](https://doc.qt.io/qt-6.8/qjsengine.html#ObjectOwnership-enum)
