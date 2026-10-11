#include "AppBootstrapper.h"
#include "LoggingRuntime.h"
#include <ExampleLog.h>
#include <QGuiApplication>
#include <QQmlExtensionPlugin>

Q_IMPORT_QML_PLUGIN(CaliburnMicroQtPlugin)
Q_IMPORT_QML_PLUGIN(CaliburnExampleModulePlugin)

int main(int argc, char *argv[])
{
    // 比 app 更早构造，保证日志出口覆盖 app 的析构。
    LoggingRuntime logging;
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("CaliburnMicroQt"));
    QCoreApplication::setApplicationName(QStringLiteral("CaliburnExampleApp"));
    logging.initialize();
    qCInfo(exampleLog).noquote() << "应用启动，日志文件："
                               << (logging.filePath().isEmpty() ? QStringLiteral("仅控制台") : logging.filePath());
    AppBootstrapper bootstrapper(app);
    const int exitCode = bootstrapper.run();
    qCInfo(exampleLog) << "应用退出，退出码：" << exitCode;
    return exitCode;
}
