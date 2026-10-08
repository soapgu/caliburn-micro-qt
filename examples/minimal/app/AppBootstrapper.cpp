#include "AppBootstrapper.h"
#include "ViewModelComposition.h"
#include <CaliburnMicroQt/ViewRegistry.h>
#include <QQuickStyle>

bool AppBootstrapper::Configure()
{
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    return RegisterRootFactory<ShellViewModel>([] { return buildShell(); })
        && ViewRegistry::registerView<ShellViewModel>(QUrl(QStringLiteral("qrc:/qt/qml/CaliburnExample/views/ShellView.qml")))
        && ViewRegistry::registerView<HomeViewModel>(QUrl(QStringLiteral("qrc:/qt/qml/CaliburnExample/views/HomeView.qml")))
        && ViewRegistry::registerView<DetailViewModel>(QUrl(QStringLiteral("qrc:/qt/qml/CaliburnExample/views/DetailView.qml")));
}

bool AppBootstrapper::OnStartup()
{
    return DisplayRootViewFor<ShellViewModel>();
}
