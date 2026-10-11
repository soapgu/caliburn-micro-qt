# Boost.Ext.DI 依赖来源与许可证

采用官方 v1.3.2 版本，由 `cmake/ExampleDependencies.cmake` 使用 FetchContent 下载官方单头文件并校验 SHA-256。不再随仓库保存头文件副本。许可证为 Boost Software License 1.0，本目录保留许可证。

- 来源：https://raw.githubusercontent.com/boost-ext/di/v1.3.2/include/boost/di.hpp
- 许可证来源：https://www.boost.org/LICENSE_1_0.txt（官方标准许可证）
- 头文件 SHA-256：1680ca33ffa04457edcea5c14346ad6e3138d8bb34d4fe074a59e13e34b54827
- CMake 配置时自动下载单头文件，不下载完整仓库或执行上游 CMake；校验头文件与原副本内容一致，不修改版本宏；缓存及离线方法见 [日志接入](../../docs/日志接入.md#依赖下载与离线构建)。
- 仅示例装配库通过 PRIVATE 链接 Caliburn::BoostDI，通用框架与用户 VM 不依赖 DI。
- 许可证 SHA-256：c9bff75738922193e67fa726fa225535870d2aa1059f91452c411736284ad566
