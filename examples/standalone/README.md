# 独立消费工程

这是独立的 CMake 项目，只接入 Caliburn.Micro.Qt 框架，不链接框架内置示例、Boost.Ext.DI 或 spdlog。Shell 根窗口使用约定定位，Home 页面由 ViewHost 装配，重置使用框架内嵌的确认窗口。

在本目录或复制到仓库外后执行；Qt kit 和框架路径由本机提供：

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH="$QT_ROOT" \
  -DCALIBURN_SOURCE_DIR="$CALIBURN_SOURCE" \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build build --parallel 4
cmake --build build --target all_qmllint
ctest --test-dir build --output-on-failure
```

Qt Creator 打开本目录的 `CMakeLists.txt`，选择 Qt 6.8.3 或更高版本的桌面 Kit；在 CMake 配置中设置 `CALIBURN_SOURCE_DIR` 为框架仓库路径，选择 `CaliburnConsumerApp` 后按 Ctrl+R。`BUILD_TESTING=OFF` 可关闭集成测试和 Qt Test 依赖。

Linux 运行 `build/bin/CaliburnConsumerApp`，macOS 运行 `build/bin/CaliburnConsumerApp.app`。CTest 使用离屏软件渲染，验证真实 QML 资源及绑定，不替代桌面 GUI 人工验收。应用运行时仍需要已安装的 Qt 动态库和平台插件。
