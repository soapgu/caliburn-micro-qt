#include "AppBootstrapper.h"
#include "ViewModelComposition.h"
#include <CaliburnMicroQt/ViewLocator.h>
#include <QQuickStyle>

bool AppBootstrapper::configure()
{
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    return registerRootFactory<ShellViewModel>([](std::shared_ptr<IWindowManager> windows) {
            return buildShell(std::move(windows));
        })
        && ViewLocator::addSearchRoot({QUrl(QStringLiteral("qrc:/qt/qml/CaliburnExample/"))});
}

void AppBootstrapper::onStartup()
{
    if (!displayRootViewFor<ShellViewModel>())
        return;
}
