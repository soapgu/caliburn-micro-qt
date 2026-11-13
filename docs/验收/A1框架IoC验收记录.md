# A1 框架 IoC 验收记录

日期：2026-11-13。框架基线：`19e45623c7fa8253bbbb7f40d2e915620c4e3b8d`。本轮完成框架配套，应用 A1 生产接入仍待实施。

## 范围与兼容边界

完成默认 unique_ptr 独占契约、Bootstrapper 根持有简化、IoC::Get/Create 门面及启动绑定／退出解绑。不增加 onWindowsReleased 或其他退出钩子，不改变辅助 VM 的既有成员释放方式，不修改 Conductor、ViewHost、View 借用及延迟回收实现。

开始时框架已有 F2 验收文档修改，红厅应用已有 Provider、Composition 和测试等未提交改动。已保存文件哈希快照并在结束前复核全部保持原样；应用侧仅同步 A1 设计文档中的辅助对象释放条款，没有修改生产代码和测试。本轮未提交推送。

这是新 Provider 独占 API 的有意收缩：已删除 OwnedInstance 别名，不提供兼容别名或特殊删除器入口。旧根工厂和默认 unique_ptr 的窗口／Conductor 调用继续保持源码兼容；使用原 OwnedInstance 或非虚析构基类独占映射的适配器须调整并重建，不声明 ABI 兼容。

## 已落地接口与所有权

| 接口／实现 | 当前行为 |
| --- | --- |
| IInstanceProvider::create<T>(name) | 返回默认 std::unique_ptr<T> |
| UniqueInstance::from<Contract> | 默认 unique_ptr 转入类型化 holder；相同类型可默认删除，跨类型须指针可转换且契约可析构、有虚析构函数 |
| 运行时校验 | 契约身份及空值校验成功后才恢复 holder 类型并移出所有权；失败由 holder 虚析构自动回收 |
| 共享解析 | 保留原 shared_ptr 控制块，不增加虚析构约束 |
| Bootstrapper 根 | std::unique_ptr<QObject>，旧工厂直接移动，提供器使用标准 unique_ptr 转换；无删除器闭包或转接 |
| IoC::Get<T>(name) | shared_ptr<T>，转发当前 Provider 的 resolve |
| IoC::Create<T>(name) | unique_ptr<T>，转发当前 Provider 的 create |

IoC 仅保存弱 Provider 引用和绑定身份，不保存业务对象表、缓存或具体容器。Bootstrapper 强持有 Provider，并用内部 RAII 凭据管理绑定；没有公开绑定 setter。请求必须在应用线程执行，错误线程先被拒绝，不访问全局上下文。未绑定、解绑或弱引用失效使用 CreationFailed，携带原请求信息；Provider 错误继续原样传播。

configure 只登记，不发布。配置成功后在窗口服务解析前绑定；活动绑定冲突时不解析新 Provider，不覆盖原绑定。未取得凭据的失败实例不能清除其他上下文。

退出沿用原顺序：准备退出 → 根关闭／onExit → 窗口、View、引擎及借用释放 → 根和根工厂释放 → 断开连接 → 按身份解绑 → 释放服务和 Provider 引用。清理异常继续解绑，基类析构兜底不调用派生退出钩子。应用辅助对象仍按原成员或共享所有权自然释放，不要求增加显式清理节点。

## 测试证据

| 范围 | 验证方式及结果 |
| --- | --- |
| 默认独占删除 | instance_provider 检查具体非虚析构类型、虚析构契约、多重继承偏移、独立创建、移动和覆盖赋值、提供器销毁后仍持有对象、恰好析构一次 |
| 错误结果 | 错误契约、空对象和异常不丢失所有权；Other 的错误结果与正确具体创建均记录析构次数 |
| 共享非虚析构接口 | 原 sharedInterfaceAdjustmentAndLifetime 保留，控制块、接口偏移和释放一次通过 |
| 非法映射编译拒绝 | 配置时 try_compile 先编译 SAFE_MAPPING 正对照，再编译同一源码的非虚析构版本；后者必须失败且诊断包含“跨类型独占映射要求契约具有虚析构函数” |
| 门面访问 | 新增 ioc 项含 4 个测试方法：未绑定及错误元数据、转发与解绑、弱引用失效和绑定冲突、错误线程和异常作用域回收 |
| 启动与清理 | 原 42 个提供器场景均增加配置不发布、服务解析时已绑定、run 后门面不可用等断言；所有根析构时可获取同一 marker，旧根工厂及其捕获对象析构时也可获取 |
| 冲突与旧路径 | 新增 bindingConflict，实际第二个 Bootstrapper 失败且零解析，第一实例仍可访问原 Provider；noProviderDoesNotBind 不为旧路径自动发布容器 |
| 连续与异常 | 既有配置／解析／激活／启动／根关闭／退出／服务清理异常及恢复场景均通过，不遗留绑定；框架原关闭守卫、View 借用、Conductor 和延迟回收测试通过 |

编译对照日志位于 `build/debug/tests/unique-mapping-safe.log` 和 `unique-mapping-unsafe.log`。正对照排除环境失败，负对照明确命中契约 static_assert，而非其他编译错误。

首次 IoC 测试编译中，QtTest 宏将圆括号临时 Binding 表达式解析为声明，改用花括号构造后编译通过；没有因此修改生产绑定逻辑。修正后定向和完整测试均通过。

## 命令与结果

环境：macOS arm64、Qt 6.8.3、CMake 3.22.1、Ninja 1.12.1、Debug。依赖版本与校验不变，构建命令显式传递 HTTP_PROXY／HTTPS_PROXY 及其小写变量，值均为 `http://127.0.0.1:7897`。GUI 验证沿用 offscreen/software/Basic，不进行真实桌面操作。

```sh
cmake --build --preset debug
ctest --test-dir build/debug \
  -R '^(ioc|instance_provider|bootstrapper_provider_)' --output-on-failure
ctest --preset debug
cmake --build build/debug --target all_qmllint

cmake -S . -B build/a1-framework-only -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_PREFIX_PATH=/Users/guhui/Qt/6.8.3/macos \
  -DCALIBURN_BUILD_EXAMPLE=OFF -DCALIBURN_BUILD_TESTS=OFF
cmake --build build/a1-framework-only --parallel 4
cmake --build build/a1-framework-only --target all_qmllint

cmake -S examples/standalone -B build/a1-standalone -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_PREFIX_PATH=/Users/guhui/Qt/6.8.3/macos \
  -DCALIBURN_SOURCE_DIR=/Users/guhui/Githubs/caliburn-micro-qt \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build build/a1-standalone --parallel 4
cmake --build build/a1-standalone --target all_qmllint
ctest --test-dir build/a1-standalone --output-on-failure
```

| 检查 | 结果 |
| --- | --- |
| Debug 构建及编译正负对照 | 通过 |
| 定向 CTest | 46/46 通过，3.02 秒 |
| 完整框架 CTest | 156/156 通过，34.21 秒；原 153 项保持通过 |
| 框架与三个示例 QML 检查 | 通过 |
| 纯框架配置、构建、QML 检查 | 均退出 0；构建 12.33 秒 |
| 独立消费工程配置、构建、QML 检查 | 均退出 0；构建 20.01 秒 |
| standalone_consumer | 1/1 通过 |
| 旧 minimal 示例 | 完整 CTest 中 bootstrapper_example、根启动及关闭场景通过，源码未迁移 |
| multiview 二进制 | --auto-close 500，0.88 秒，退出 0，无超时 |
| multiscreen 二进制 | --auto-close 3，3.30 秒，退出 0，预览首帧和窗口检查通过，无超时 |
| git diff --check | 通过 |

兼容性命令与退出码记录在 `build/a1-ioc-logs/compat-results.json`，对应 stdout／stderr 单独保存。构建及回归日志为 build.log、targeted.log、ctest.log、qmllint.log；既有文件保护快照和复核结果为 before.json、preserved.json。以上均在忽略的构建目录，不提交产物。

## 交付状态

A1 框架配套已完成本机验收。不扩展所有权回收机制，不新增退出钩子。应用 Provider 内部唯一 Boost.DI、删除 CheckInComposition、报到页和矩阵生产统一入口仍留待应用批次；本轮不声称应用 A1 已完成。麒麟、真实设备、现场四屏及真实桌面未验证，历史 F1／F2 记录不替代本轮结果。
