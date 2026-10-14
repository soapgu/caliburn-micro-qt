#pragma once

#include "ShellViewModel.h"
#include <CaliburnMicroQt/BootstrapperBase.h>
#include <CaliburnMicroQt/ViewLocator.h>
#include <QQuickStyle>

class AppBootstrapper : public BootstrapperBase
{
public:
    using BootstrapperBase::BootstrapperBase;
protected:
    bool configure() override
    {
        QQuickStyle::setStyle(QStringLiteral("Basic"));
        return registerRootFactory<ShellViewModel>([](std::shared_ptr<IWindowManager> windows) {
            return std::make_unique<ShellViewModel>(std::move(windows));
        }) && ViewLocator::addSearchRoot({QUrl(QStringLiteral("qrc:/qt/qml/CaliburnConsumer/"))});
    }
    void onStartup() override { (void)displayRootViewFor<ShellViewModel>(); }
};
