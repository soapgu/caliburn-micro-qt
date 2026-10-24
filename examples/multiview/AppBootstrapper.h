#pragma once
#include "ViewModels.h"
#include <CaliburnMicroQt/BootstrapperBase.h>
#include <CaliburnMicroQt/IWindowManager.h>
#include <CaliburnMicroQt/ViewLocator.h>
#include <QGuiApplication>
#include <QQuickStyle>
#include <QTimer>

namespace MultiView {
class AppBootstrapper final : public BootstrapperBase
{
public:
    explicit AppBootstrapper(QGuiApplication &app, int autoCloseMs = 0)
        : BootstrapperBase(app), m_autoCloseMs(autoCloseMs) {}
protected:
    bool configure() override
    {
        QQuickStyle::setStyle(QStringLiteral("Basic"));
        return ViewLocator::addSearchRoot({QUrl(QStringLiteral("qrc:/qt/qml/CaliburnMultiView/")),
                                           QStringLiteral("MultiView")})
            && registerRootFactory<ShellViewModel>([this](std::shared_ptr<IWindowManager>) {
                auto shell = std::make_unique<ShellViewModel>();
                m_shell = shell.get();
                return shell;
            });
    }
    void onStartup() override
    {
        if (displayRootViewFor<ShellViewModel>() && m_autoCloseMs > 0)
            QTimer::singleShot(m_autoCloseMs, m_shell, &ShellViewModel::exitApplication);
    }
private:
    int m_autoCloseMs;
    QPointer<ShellViewModel> m_shell;
};
}
