#include "AppBootstrapper.h"
#include <QGuiApplication>
#include <QQmlExtensionPlugin>

Q_IMPORT_QML_PLUGIN(CaliburnMicroQtPlugin)
Q_IMPORT_QML_PLUGIN(CaliburnConsumerModulePlugin)

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("CaliburnConsumerApp"));
    AppBootstrapper bootstrapper(app);
    return bootstrapper.run();
}
