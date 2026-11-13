# Provider 模板化验收

日期：2026-11-13。本地框架基于 f0678dc502e13c94978d315ee10637c3284734eb 的未提交工作区，应用基于 e7790b50c4c8b574916d518a32f97234ac7defae 的未提交工作区。两仓库均未暂存、提交或推送；结果限定本次本机原生 arm64 验证。

## 实现

- 非模板 IInstanceProvider 保留虚析构与固定 windowManager()；新增 InstanceProvider<Injector> 直接提供 resolve<T>()／create<T>()。
- TypedIoC<Provider> 复用唯一全局基类弱引用，静态转换后直接调用模板。无请求名称、集合、类型白名单、结果包装及校验，构造异常原样传播。
- Bootstrapper 安装接口不变，通过固定契约获取窗口服务；根模型统一显式工厂。配置后绑定、根与工厂清理后解绑，线程和窗口生命周期校验保留。
- 框架测试使用独立强类型测试后端；旧类型擦除／名称／集合测试及 unique_mapping 编译对照随已删除接口移除。窗口服务和根模型的错误类型返回在新签名下无法表达，原两项测试删除；空值、构造异常、线程、父对象和回收测试继续覆盖。

## 验证

```sh
/usr/bin/arch -arm64 cmake --build build/debug --parallel 4
/usr/bin/arch -arm64 ctest --test-dir build/debug --output-on-failure --parallel 4
```

框架构建成功，CTest **154/154 通过，15.38 秒**。包含模板创建与所有权、IoC 绑定及原始异常、Bootstrapper 失败清理／冲突／连续上下文、原有示例、窗口、ViewHost、Conductor 和多视图测试。

日志：build/provider-template-build.log、build/provider-template-regression-build.log、build/provider-template-ctest.log、build/provider-template-regression-ctest.log。首次 44 项专项测试全部通过，随后全量回归通过。

应用原生 arm64 构建成功，相关 **17/17 通过，6.46 秒**；详见应用 [Provider 模板化验收](../../../../GitLabs/project_expo/red-conf-system/docs/Provider模板化验收.md)。未执行麒麟部署、现场四屏或真实设备验收。
