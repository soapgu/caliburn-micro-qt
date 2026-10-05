#include <ShellViewModel.h>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlExtensionPlugin>
#include <QQuickStyle>
#include <memory>

Q_IMPORT_QML_PLUGIN(CaliburnMicroQtPlugin)
Q_IMPORT_QML_PLUGIN(CaliburnExampleModulePlugin)

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    auto shell = std::make_unique<ShellViewModel>();
    QQmlEngine::setObjectOwnership(shell.get(), QQmlEngine::CppOwnership);
    QQmlApplicationEngine engine;
    engine.setInitialProperties({{QStringLiteral("viewModel"), QVariant::fromValue(shell.get())}});
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/CaliburnExample/views/ShellView.qml")));
    if (engine.rootObjects().isEmpty())
        return 1;
    return app.exec();
}
