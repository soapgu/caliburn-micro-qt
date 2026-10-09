#include "AppBootstrapper.h"
#include "ViewModelComposition.h"
#include <CaliburnMicroQt/ViewRegistry.h>
#include <QQuickStyle>
#include <CaliburnMicroQt/ConfirmActionViewModel.h>

bool AppBootstrapper::Configure()
{
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    return ViewRegistry::registerView<ConfirmActionViewModel>(QUrl(QStringLiteral("qrc:/qt/qml/Caliburn/Micro/Qt/ConfirmActionView.qml")))
        && RegisterRootFactory<ShellViewModel>([] { return buildShell(); })
        && ViewRegistry::registerView<ShellViewModel>(QUrl(QStringLiteral("qrc:/qt/qml/CaliburnExample/views/ShellView.qml")))
        && ViewRegistry::registerView<HomeViewModel>(QUrl(QStringLiteral("qrc:/qt/qml/CaliburnExample/views/HomeView.qml")))
        && ViewRegistry::registerView<DetailViewModel>(QUrl(QStringLiteral("qrc:/qt/qml/CaliburnExample/views/DetailView.qml")));
}

bool AppBootstrapper::OnStartup()
{
    return DisplayRootViewFor<ShellViewModel>();
}
