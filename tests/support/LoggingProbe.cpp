#include "LoggingRuntime.h"
#include "ExampleLog.h"
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <cstdio>
#include <memory>
#include <thread>
#include <vector>

namespace {
void previousHandler(QtMsgType, const QMessageLogContext &, const QString &message)
{
    std::fprintf(stderr, "旧处理器：%s\n", message.toUtf8().constData());
}

struct DestructionLog
{
    ~DestructionLog() { qCInfo(exampleLog) << "析构日志"; }
};
}

class LoggingLocationProbe
{
public:
    void record() { qCInfo(exampleLog).noquote() << "中文正文 {} %1 %s"; }
};

int main(int argc, char **argv)
{
    LoggingRuntime logging;
    QGuiApplication app(argc, argv);
    QObject::connect(&app, &QObject::destroyed, [] { qCInfo(exampleLog) << "应用析构日志"; });
    QCoreApplication::setOrganizationName(QStringLiteral("CaliburnMicroQt"));
    QCoreApplication::setApplicationName(QStringLiteral("CaliburnExampleApp"));
    const auto arguments = app.arguments();
    if (arguments.size() != 3) return 2;
    const auto scenario = arguments.at(1);
    LoggingOptions options;
    options.filePath = arguments.at(2);
    if (scenario == QStringLiteral("optionsAlways")) options.consoleColor = ConsoleColorMode::Always;
    if (scenario == QStringLiteral("optionsNever")) options.consoleColor = ConsoleColorMode::Never;
    if (scenario == QStringLiteral("rotation")) options.maxFileSize = 1024;
    if (scenario == QStringLiteral("restore")) {
        const auto original = qInstallMessageHandler(previousHandler);
        {
            LoggingRuntime temporary;
            temporary.initialize(options);
            qCInfo(exampleLog) << "桥接中的消息";
        }
        qCInfo(exampleLog) << "恢复后的消息";
        qInstallMessageHandler(original);
        return 0;
    }
    logging.initialize(options);
    DestructionLog destruction;
    if (scenario == QStringLiteral("fatal")) qFatal("终止前日志");
    if (scenario == QStringLiteral("threads")) {
        std::vector<std::thread> workers;
        for (int worker = 0; worker < 4; ++worker)
            workers.emplace_back([worker] {
                for (int index = 0; index < 25; ++index)
                    qCInfo(exampleLog).noquote() << QStringLiteral("并发-%1-%2").arg(worker).arg(index);
            });
        for (auto &worker : workers) worker.join();
    } else if (scenario == QStringLiteral("rotation")) {
        for (int index = 0; index < 80; ++index)
            qCInfo(exampleLog) << "轮转消息，填充文件内容" << index;
    } else {
        LoggingLocationProbe{}.record();
        qCDebug(exampleLog) << "调试消息";
        qCInfo(exampleLog) << "信息消息";
        qCWarning(exampleLog) << "警告消息";
        qCCritical(exampleLog) << "错误消息";
        qWarning("Qt 原生消息");
        QLoggingCategory internal("qt.caliburn.loggingprobe");
        qCDebug(internal) << "Qt 内部调试消息";
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.setData("import QtQml\nQtObject { Component.onCompleted: console.info('QML 原生消息') }",
                          QUrl(QStringLiteral("qrc:/LoggingProbe.qml")));
        std::unique_ptr<QObject> object(component.create());
        if (!object) return 3;
    }
    return 0;
}
