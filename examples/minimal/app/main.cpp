#include "ViewModelComposition.h"
#include <CaliburnMicroQt/ViewRegistry.h>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlExtensionPlugin>
#include <QQuickStyle>
#include <memory>
#include <exception>

Q_IMPORT_QML_PLUGIN(CaliburnMicroQtPlugin)
Q_IMPORT_QML_PLUGIN(CaliburnExampleModulePlugin)

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    std::unique_ptr<ShellViewModel> shell;
    try {
        shell = buildShell();
    } catch (const std::exception &error) {
        qCritical().noquote() << "示例装配失败：" << error.what();
        return 1;
    }

    if (!ViewRegistry::registerView<ShellViewModel>(QUrl(QStringLiteral("qrc:/qt/qml/CaliburnExample/views/ShellView.qml")))
            || !ViewRegistry::registerView<HomeViewModel>(QUrl(QStringLiteral("qrc:/qt/qml/CaliburnExample/views/HomeView.qml"))))
        return 1;
    shell->initialize();
    shell->activate();
    const auto shutdown = [&shell] {
        shell->deactivate(true);
    };
    QObject::connect(&app, &QCoreApplication::aboutToQuit, shell.get(), shutdown);

    QQmlApplicationEngine engine;
    engine.setInitialProperties({{QStringLiteral("viewModel"), QVariant::fromValue(shell.get())}});
    engine.load(ViewRegistry::viewUrl(shell.get()));
    if (engine.rootObjects().isEmpty()) {
        shutdown();
        return 1;
    }
    return app.exec();
}
