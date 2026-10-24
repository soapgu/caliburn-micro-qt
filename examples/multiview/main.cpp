#include "AppBootstrapper.h"
#include <QCommandLineParser>
#include <QQmlExtensionPlugin>

Q_IMPORT_QML_PLUGIN(CaliburnMicroQtPlugin)
Q_IMPORT_QML_PLUGIN(CaliburnMultiViewModulePlugin)

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("CaliburnMultiViewApp"));
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("同一 HomeViewModel 的 context 多视图演示"));
    parser.addHelpOption();
    parser.addOption({QStringLiteral("auto-close"), QStringLiteral("指定毫秒数后请求关闭窗口"), QStringLiteral("milliseconds")});
    parser.process(app);
    int autoCloseMs = 0;
    if (parser.isSet(QStringLiteral("auto-close"))) {
        bool ok = false;
        autoCloseMs = parser.value(QStringLiteral("auto-close")).toInt(&ok);
        if (!ok || autoCloseMs <= 0) return 2;
    }
    MultiView::AppBootstrapper bootstrapper(app, autoCloseMs);
    return bootstrapper.run();
}
