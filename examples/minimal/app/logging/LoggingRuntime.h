#pragma once

#include <QString>
#include <QtLogging>
#include <cstddef>
#include <memory>

// example 的输出配置；不是框架 API。
struct LoggingOptions
{
    QString filePath;
    std::size_t maxFileSize = 5 * 1024 * 1024;
    std::size_t maxFiles = 3;
};

class LoggingRuntime final
{
public:
    LoggingRuntime() = default;
    ~LoggingRuntime();
    LoggingRuntime(const LoggingRuntime &) = delete;
    LoggingRuntime &operator=(const LoggingRuntime &) = delete;

    // 应用信息设置完成后在主线程调用一次。返回是否启用了文件输出；
    // 文件失败时仍安装控制台输出。进程内只允许一个实例接管处理器。
    bool initialize(const LoggingOptions &options = {});
    QString filePath() const;

private:
    struct State;
    static void handleMessage(QtMsgType type, const QMessageLogContext &context,
                              const QString &message) noexcept;
    static std::shared_ptr<State> s_active;
    std::shared_ptr<State> m_state;
};
