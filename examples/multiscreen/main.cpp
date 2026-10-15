#include "AppBootstrapper.h"
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlExtensionPlugin>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <algorithm>

Q_IMPORT_QML_PLUGIN(CaliburnMicroQtPlugin)
Q_IMPORT_QML_PLUGIN(CaliburnMultiScreenModulePlugin)

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("CaliburnMultiScreenApp"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0"));
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("CM 双窗口多屏示例：自动适配四屏或本机预览。"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({QStringLiteral("strict-screens"), QStringLiteral("严格要求 X11 四屏布局，否则退出。")});
    parser.addOption({QStringLiteral("auto-close"), QStringLiteral("指定秒数后请求关闭主窗（3～86400）。"), QStringLiteral("seconds")});
    parser.process(app);
    int autoClose = 0;
    if (parser.isSet(QStringLiteral("auto-close"))) {
        auto seconds = MultiScreen::parseAutoClose(parser.value(QStringLiteral("auto-close")));
        if (!seconds) { qCritical() << "Auto-close seconds must be between 3 and 86400"; return 2; }
        autoClose = *seconds;
    }
    auto screens = QGuiApplication::screens();
    std::sort(screens.begin(), screens.end(), [](QScreen *a, QScreen *b) { return a->geometry().x() < b->geometry().x(); });
    QList<MultiScreen::ScreenMetrics> metrics;
    qInfo() << "Qt:" << qVersion() << "platform:" << QGuiApplication::platformName();
    for (auto *screen : screens) {
        metrics.append({screen->geometry(), screen->availableGeometry(), screen->devicePixelRatio()});
        qInfo() << "Screen:" << screen->name() << screen->geometry() << "DPR:" << screen->devicePixelRatio();
    }
    auto layout = MultiScreen::selectLayout(QGuiApplication::platformName(), metrics,
                                           parser.isSet(QStringLiteral("strict-screens")));
    if (!layout) { qCritical() << "Required screen layout unavailable"; return 2; }
    qInfo() << "Mode:" << (layout->fourScreen ? "four-screen" : "preview (not four-screen acceptance)");
    if (qgetenv("QT_QUICK_BACKEND") != "software")
        QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    MultiScreen::AppBootstrapper bootstrapper(app, *layout, autoClose);
    const int result = bootstrapper.run();
    qInfo() << "Application exited; exit code:" << result;
    return result;
}
