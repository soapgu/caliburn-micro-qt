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
    qCInfo(exampleLog).noquote() << "Application started; log file:"
                               << (logging.filePath().isEmpty() ? QStringLiteral("console only") : logging.filePath());
    AppBootstrapper bootstrapper(app);
    const int exitCode = bootstrapper.run();
    qCInfo(exampleLog) << "Application exited; exit code:" << exitCode;
    return exitCode;
}
