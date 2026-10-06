#include "AppBootstrapper.h"
#include <QGuiApplication>
#include <QQmlExtensionPlugin>

Q_IMPORT_QML_PLUGIN(CaliburnMicroQtPlugin)
Q_IMPORT_QML_PLUGIN(CaliburnExampleModulePlugin)

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    AppBootstrapper bootstrapper(app);
    return bootstrapper.Run();
}
