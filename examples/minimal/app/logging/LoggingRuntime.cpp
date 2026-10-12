#include "LoggingRuntime.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QtLogging>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <atomic>
#include <cstdio>
#include <mutex>
#include <stdexcept>
#include <vector>

namespace {
std::mutex installationMutex;

void reportError(const char *message) noexcept
{
    std::fprintf(stderr, "Logging output error: %s\n", message);
    std::fflush(stderr);
}

spdlog::level::level_enum levelFor(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg: return spdlog::level::debug;
    case QtInfoMsg: return spdlog::level::info;
    case QtWarningMsg: return spdlog::level::warn;
    case QtCriticalMsg: return spdlog::level::err;
    case QtFatalMsg: return spdlog::level::critical;
    }
    return spdlog::level::err;
}
}

struct LoggingRuntime::State
{
    std::shared_ptr<spdlog::logger> logger;
    QtMessageHandler previous = nullptr;
    QString filePath;
};

std::shared_ptr<LoggingRuntime::State> LoggingRuntime::s_active;

bool LoggingRuntime::initialize(const LoggingOptions &options)
{
    std::lock_guard<std::mutex> lock(installationMutex);
    if (m_state || std::atomic_load(&s_active))
        throw std::logic_error("A logging handler is already installed");

    auto state = std::make_shared<State>();
    std::vector<spdlog::sink_ptr> sinks;
    sinks.push_back(std::make_shared<spdlog::sinks::stderr_color_sink_mt>());
    QString path = options.filePath;
    if (path.isEmpty()) {
        const auto directory = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        if (!directory.isEmpty())
            path = QDir(directory).filePath(QStringLiteral("logs/CaliburnExampleApp.log"));
    }
    try {
        if (path.isEmpty() || !QDir().mkpath(QFileInfo(path).absolutePath()))
            throw std::runtime_error("Cannot create the log directory");
        sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            path.toStdString(), options.maxFileSize, options.maxFiles));
        state->filePath = QFileInfo(path).absoluteFilePath();
    } catch (const std::exception &error) {
        reportError(error.what());
    }
    // 不注册全局 logger，也不修改应用中其他 spdlog logger 的配置。
    state->logger = std::make_shared<spdlog::logger>("qt", sinks.begin(), sinks.end());
    state->logger->set_level(spdlog::level::trace);
    state->logger->set_pattern("%v");
    state->logger->set_error_handler([](const std::string &error) { reportError(error.c_str()); });
    state->logger->flush_on(spdlog::level::warn);
    qSetMessagePattern(QStringLiteral(
        "[%{time yyyy-MM-dd hh:mm:ss.zzz}] [%{type}] [tid=%{threadid}] "
        "[%{category}] %{function} [%{file}:%{line}] %{message}"));
    // 先发布状态，随后接管全局入口；工作线程只借用状态快照。
    m_state = state;
    std::atomic_store(&s_active, state);
    state->previous = qInstallMessageHandler(&LoggingRuntime::handleMessage);
    return !state->filePath.isEmpty();
}

LoggingRuntime::~LoggingRuntime()
{
    std::lock_guard<std::mutex> lock(installationMutex);
    if (!m_state) return;
    qInstallMessageHandler(m_state->previous);
    std::atomic_store(&s_active, std::shared_ptr<State>{});
    try {
        m_state->logger->flush();
    } catch (...) {
        reportError("Failed to flush logs during shutdown");
    }
    // 已经进入回调的线程仍持有自己的 shared_ptr，不会访问已释放的 sink。
}

QString LoggingRuntime::filePath() const
{
    return m_state ? m_state->filePath : QString{};
}

void LoggingRuntime::handleMessage(QtMsgType type, const QMessageLogContext &context,
                                   const QString &message) noexcept
{
    try {
        auto state = std::atomic_load(&s_active);
        const auto formatted = qFormatLogMessage(type, context, message).toUtf8();
        if (!state) {
            std::fprintf(stderr, "%s\n", formatted.constData());
            std::fflush(stderr);
            return;
        }
        // 正文作为数据传递，不将其中的花括号解释为 fmt 格式串。
        state->logger->log(levelFor(type), spdlog::string_view_t(
            formatted.constData(), static_cast<std::size_t>(formatted.size())));
        if (type == QtFatalMsg) state->logger->flush();
    } catch (const std::exception &error) {
        reportError(error.what());
    } catch (...) {
        reportError("Unknown logging output exception");
    }
}
