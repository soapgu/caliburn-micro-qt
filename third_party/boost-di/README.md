# Boost.Ext.DI 固定源码依赖

采用官方 v1.3.2 标签的 include/boost/di.hpp 单头文件，保留原文件，不修改其版本宏。许可证为 Boost Software License 1.0，随源码保存。

- 来源：https://github.com/boost-ext/di/tree/v1.3.2
- 许可证来源：https://www.boost.org/LICENSE_1_0.txt（官方标准许可证）
- 头文件 SHA-256：1680ca33ffa04457edcea5c14346ad6e3138d8bb34d4fe074a59e13e34b54827
- CMake 配置校验头文件，不在构建时下载依赖。
- 仅示例装配库通过 PRIVATE 链接 Caliburn::BoostDI，通用框架与用户 VM 不依赖 DI。
- 许可证 SHA-256：c9bff75738922193e67fa726fa225535870d2aa1059f91452c411736284ad566
