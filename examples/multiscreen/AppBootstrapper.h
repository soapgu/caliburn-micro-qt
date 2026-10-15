#pragma once
#include <CaliburnMicroQt/BootstrapperBase.h>
#include <CaliburnMicroQt/IWindowManager.h>
#include "ScreenLayout.h"
#include "ViewModels.h"

namespace MultiScreen {
class AppBootstrapper final : public BootstrapperBase
{
    Q_OBJECT
public:
    AppBootstrapper(QGuiApplication &app, ScreenLayout layout, int autoClose = 0,
                    bool verifyRendering = true);
signals:
    void windowsReady();
protected:
    bool configure() override;
    void onStartup() override;
private:
    void watchWindow(QQuickWindow *window, bool display);
    ScreenLayout m_layout;
    int m_autoClose;
    bool m_verifyRendering;
    bool m_mainRendered = false;
    bool m_displayRendered = false;
    bool m_displayClosed = false;
    std::shared_ptr<IWindowManager> m_windows;
    QPointer<ShellViewModel> m_shell;
    std::unique_ptr<DisplayViewModel> m_display;
};
}
