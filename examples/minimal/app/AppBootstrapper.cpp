#include "AppBootstrapper.h"
#include "ViewModelComposition.h"
#include <CaliburnMicroQt/ViewRegistry.h>
#include <QQuickStyle>

bool AppBootstrapper::Configure()
{
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    return RegisterRootFactory<ShellViewModel>([](std::shared_ptr<IWindowManager> windows) {
            return buildShell(std::move(windows));
        })
        && ViewRegistry::registerView<ShellViewModel>(QUrl(QStringLiteral("qrc:/qt/qml/CaliburnExample/views/ShellView.qml")))
        && ViewRegistry::registerView<HomeViewModel>(QUrl(QStringLiteral("qrc:/qt/qml/CaliburnExample/views/HomeView.qml")))
        && ViewRegistry::registerView<DetailViewModel>(QUrl(QStringLiteral("qrc:/qt/qml/CaliburnExample/views/DetailView.qml")));
}

void AppBootstrapper::OnStartup()
{
    if (!DisplayRootViewFor<ShellViewModel>())
        return;
}
