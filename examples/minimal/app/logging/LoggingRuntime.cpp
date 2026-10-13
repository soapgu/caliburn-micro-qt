#include "LoggingRuntime.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QtLogging>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#ifdef _WIN32
#include <spdlog/details/windows_include.h>
#include <wincon.h>
#endif
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

spdlog::color_mode colorModeFor(ConsoleColorMode configured)
{
    const auto value = qgetenv("CMQT_LOG_COLOR").trimmed().toLower();
    if (value == "auto") return spdlog::color_mode::automatic;
    if (value == "always") return spdlog::color_mode::always;
    if (value == "never") return spdlog::color_mode::never;
    if (!value.isEmpty())
        reportError("Invalid CMQT_LOG_COLOR; expected auto, always or never; using configured mode");
    switch (configured) {
    case ConsoleColorMode::Automatic: return spdlog::color_mode::automatic;
    case ConsoleColorMode::Always: return spdlog::color_mode::always;
    case ConsoleColorMode::Never: return spdlog::color_mode::never;
    }
    return spdlog::color_mode::automatic;
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
    auto console = std::make_shared<spdlog::sinks::stderr_color_sink_mt>(
        colorModeFor(options.consoleColor));
    // Debug 使用深灰色（Windows 为普通灰色）、Info 绿色、Warning 加粗黄色；错误使用红色前景。
#ifdef _WIN32
    console->set_color(spdlog::level::debug, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    console->set_color(spdlog::level::err, FOREGROUND_RED);
    console->set_color(spdlog::level::critical, FOREGROUND_RED | FOREGROUND_INTENSITY);
#else
    // Qt Creator 20.0.1 不识别 ANSI 90；使用其支持的 RGB 色码明确指定灰色。
    console->set_color(spdlog::level::debug, "\x1b[38;2;128;128;128m");
    console->set_color(spdlog::level::err, console->red);
    console->set_color(spdlog::level::critical, console->red_bold);
#endif
    console->set_pattern("%^%v%$");
    sinks.push_back(console);
    QString path = options.filePath;
    if (path.isEmpty()) {
        const auto directory = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        if (!directory.isEmpty())
            path = QDir(directory).filePath(QStringLiteral("logs/CaliburnExampleApp.log"));
    }
    try {
        if (path.isEmpty() || !QDir().mkpath(QFileInfo(path).absolutePath()))
            throw std::runtime_error("Cannot create the log directory");
        auto file = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            path.toStdString(), options.maxFileSize, options.maxFiles);
        file->set_pattern("%v");
        sinks.push_back(file);
        state->filePath = QFileInfo(path).absoluteFilePath();
    } catch (const std::exception &error) {
        reportError(error.what());
    }
    // 不注册全局 logger，也不修改应用中其他 spdlog logger 的配置。
    state->logger = std::make_shared<spdlog::logger>("qt", sinks.begin(), sinks.end());
    state->logger->set_level(spdlog::level::trace);
    // 各 sink 独立格式化，避免全局 pattern 覆盖控制台着色或污染文件。
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
