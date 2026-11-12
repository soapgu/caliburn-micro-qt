# IoC 解析契约验收记录

日期：2026-11-12。实现基于提交 `1050701ab9dd065b2bd3a6691ed3d484379033a1`，对应 [IoC 改造方案](../IoC改造方案.md)第 2 步。

## 实现范围

新增公开头文件 `CaliburnMicroQt/IInstanceProvider.h` 与非模板实现 `InstanceProvider.cpp`，加入现有框架目标。提供器是普通 C++ 接口，无 QObject 继承、QML 注册、全局静态容器或具体容器依赖。

| 公开方法 | 返回与行为 |
| --- | --- |
| resolve<T>(name) | shared_ptr<T>；当前请求缺失则抛 NotRegistered |
| tryResolve<T>(name) | shared_ptr<T>；仅当前请求缺失返回空，依赖错误继续传播 |
| create<T>(name) | OwnedInstance<T>，保留实际实现删除器的独占指针 |
| resolveAll<T>() | vector<shared_ptr<T>>；空登记返回空集合，包含命名与未命名项，按登记顺序且不按对象地址去重 |
| buildUp<T>(T&) | 同步借用静态契约对象执行显式注入，不接管所有权 |

名称使用 QString，区分大小写且不修剪；空名称表示未命名契约。类型采用 std::type_index，模板仅接受非 cv、非指针、非数组的对象类型，引用类型也被对象类型约束拒绝。类型身份用于当前进程，不作为持久化键。

适配器必须实现受保护的 resolveShared；nullopt 只表示当前请求缺失。createUnique、resolveCollection、injectExisting 默认抛 UnsupportedCapability。单实例注册唯一性、循环检测、集合顺序和显式注入点由适配器负责，框架不提供生产注册容器。

## 所有权与错误边界

- SharedInstance::from<Contract> 消费或复制 shared_ptr<Implementation>，在擦除类型前进行合法契约转换，保留原共享控制块及接口偏移。
- UniqueInstance::from<Contract> 仅消费默认删除器的 unique_ptr<Implementation>，先构造删除器再释放输入。结果仅可移动，保存契约指针及对实际 Implementation 的删除方式，不要求契约基类有虚析构函数。
- OwnedInstance<T> 的删除器绑定本次创建的对象。允许移动、释放或 reset(nullptr)，不得 reset 为无关的新对象，不得将删除器用于无关指针。release 后调用者仍须保留并正确调用原删除器。
- 类型校验在恢复指针前执行；结果错误或后续异常时仍由原结果回收。不得将共享或借用对象包装为独占结果。接口不提供此类转换入口。
- BorrowedInstance::as<T> 验证契约类型后返回引用，适配器不得保留该借用包装；不扫描 Q_PROPERTY、不发现动态契约。
- InstanceError 保存 code、operation、request、dependencyChain。错误码包含 NotRegistered、TypeMismatch、DuplicateContract、CircularDependency、UnsupportedCapability、CreationFailed。
- 已登记空结果报告 CreationFailed，错误契约报告 TypeMismatch。已有 InstanceError 原样传播；其他标准异常与未知异常转换为 CreationFailed，并以 std::throw_with_nested 保留原始异常。

提供器无线程安全保证，QObject 创建线程由应用适配器负责。本阶段不接入 Bootstrapper，不修改 WindowManager 或旧根工厂。后续根对象接入必须保留独占结果的删除器，不能直接改包装为默认删除器的 unique_ptr<QObject>。

## 测试适配器与覆盖

`tests/support/TestInstanceProvider.h` 仅由新增测试使用，不进入框架发布目标。它区分单实例登记 add 与集合追加 append：单实例按类型与名称去重，集合追加允许重复契约及同一对象；集合按两类登记的原始顺序返回。解析栈通过作用域退出清理，记录循环及依赖缺失链。

新增 CTest 项 `instance_provider`，由 `tests/cpp/tst_instanceprovider.cpp` 执行以下 9 个测试方法：

| 方法 | 核心断言 |
| --- | --- |
| sharedIdentityAndNames | 普通服务、共享指针与控制块身份、名称大小写与空格、类型区分、必需／可选缺失及错误元数据 |
| sharedInterfaceAdjustmentAndLifetime | 多重继承的非零接口偏移、控制块保留、提供器销毁后对象继续存活，最后引用释放时析构一次 |
| uniqueAdjustmentMoveAndLifetime | 非虚析构契约、多重继承偏移、不同创建结果独立、移动及覆盖赋值回收、提供器销毁后独占结果仍有效 |
| wrongTypesAndEmptyResults | 必需与可选共享查询的错误类型和空结果；独占错误类型回收一次及空结果拒绝 |
| collectionOrderAndValidation | 空集合、命名及未命名项、登记顺序、同指针重复项保留、错误类型及空集合元素拒绝 |
| buildUpAndUnsupportedCapabilities | 注入原对象、地址与存储保持不变、借用类型错误、默认三项能力明确不支持 |
| duplicatesCyclesAndDependencyErrors | 重复单实例、可选查询不能吞依赖缺失、依赖链、循环检测、异常后解析栈恢复 |
| preservesAdapterError | 框架原样保留适配器 InstanceError 的操作、类型、名称、消息及完整依赖链 |
| exceptionsAndContextIsolation | 标准及未知嵌套异常、BuildUp 与集合异常操作标识、不同提供器上下文相互隔离 |

## 环境、命令及结果

环境：macOS arm64，Qt 6.8.3，CMake 3.22.1、Ninja 1.12.1，Debug。复用匹配当前仓库的 build/debug。GUI 回归采用现有 offscreen/software 配置；新增契约测试使用 QCoreApplication，不依赖桌面窗口。

在仓库根目录执行：

```sh
cmake --build --preset debug
ctest --test-dir build/debug -R '^instance_provider$' --output-on-failure
ctest --preset debug
cmake --build build/debug --target all_qmllint
cmake -S . -B build/ioc-contract-framework-only -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH=/Users/guhui/Qt/6.8.3/macos \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCALIBURN_BUILD_EXAMPLE=OFF -DCALIBURN_BUILD_TESTS=OFF
cmake --build build/ioc-contract-framework-only
git diff --check
```

| 检查 | 结果 |
| --- | --- |
| Debug 构建 | 通过 |
| instance_provider 定向测试 | 1/1 通过，0.10 秒 |
| 完整 CTest | 111/111 通过，33.27 秒；原有 110 项全部保持通过 |
| all_qmllint | 通过 |
| 关闭示例与测试的纯框架配置及构建 | 通过，无 Boost.DI 缓存配置项 |
| git diff --check | 通过 |

实际输出保留在忽略的构建目录 `build/debug/ioc-contract/`：build.log、targeted.log、ctest.log、qmllint.log、framework-configure.log、framework-build.log。详细测试输出可查看 build/debug/Testing/Temporary/LastTest.log，后续测试会覆盖该文件。

本阶段未发现生产缺陷；未修改既有启动、窗口、视图定位或生命周期实现。未测试实际容器接入、Bootstrapper 新入口、真实桌面、四屏、麒麟平台和下游红厅 F2 验证；不承诺二进制 ABI 兼容。本阶段未提交推送。
