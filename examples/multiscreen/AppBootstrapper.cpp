#include "AppBootstrapper.h"
#include <CaliburnMicroQt/ViewLocator.h>
#include <QGuiApplication>
#include <QQuickWindow>
#include <QTimer>
#include <algorithm>
#include <stdexcept>

namespace MultiScreen {
AppBootstrapper::AppBootstrapper(QGuiApplication &app, ScreenLayout layout, int autoClose,
                                 bool verifyRendering)
    : BootstrapperBase(app), m_layout(layout), m_autoClose(autoClose), m_verifyRendering(verifyRendering) {}

bool AppBootstrapper::configure()
{
    return ViewLocator::addSearchRoot({QUrl(QStringLiteral("qrc:/qt/qml/CaliburnMultiScreen/")),
                                        QStringLiteral("MultiScreen")})
        && registerRootFactory<ShellViewModel>([this](std::shared_ptr<IWindowManager> windows) {
            m_windows = std::move(windows);
            auto shell = std::make_unique<ShellViewModel>(m_layout.fourScreen);
            m_shell = shell.get();
            return shell;
        });
}
void AppBootstrapper::watchWindow(QQuickWindow *window, bool display)
{
    connect(window, &QQuickWindow::frameSwapped, this, [this, display] {
        bool &rendered = display ? m_displayRendered : m_mainRendered;
        if (!rendered) {
            rendered = true;
            qInfo() << (display ? "Display first frame submitted" : "Main first frame submitted");
        }
    });
    connect(window, &QQuickWindow::sceneGraphError, this,
        [](QQuickWindow::SceneGraphError error, const QString &message) {
            qCritical() << "Rendering failed:" << error << message;
            QCoreApplication::exit(4);
        });
}
void AppBootstrapper::onStartup()
{
    auto screens = QGuiApplication::screens();
    std::sort(screens.begin(), screens.end(), [](QScreen *a, QScreen *b) {
        return a->geometry().x() < b->geometry().x();
    });
    RootViewOptions rootOptions;
    rootOptions.window.screen = screens.front();
    rootOptions.window.geometry = m_layout.mainGeometry;
    rootOptions.window.activateOnShow = true;
    if (!displayRootViewFor<ShellViewModel>(rootOptions)) return;
    QPointer<QQuickWindow> main = m_windows->windowFor(m_shell);
    if (!main || !main->isVisible()) return;
    watchWindow(main, false);
    m_display = std::make_unique<DisplayViewModel>(m_layout.fourScreen);
    WindowOptions displayOptions;
    displayOptions.role = WindowRole::Auxiliary;
    displayOptions.screen = screens[m_layout.fourScreen ? 1 : 0];
    displayOptions.geometry = m_layout.displayGeometry;
    displayOptions.visibility = QWindow::Windowed;
    if (m_layout.fourScreen) displayOptions.flags = Qt::Window | Qt::FramelessWindowHint
        | Qt::WindowStaysOnTopHint | Qt::X11BypassWindowManagerHint
        | Qt::WindowDoesNotAcceptFocus | Qt::WindowTransparentForInput;
    if (!m_windows->showWindow(QVariant::fromValue(m_display.get()), displayOptions))
        throw std::runtime_error("Failed to show the auxiliary display window");
    QPointer<QQuickWindow> display = m_windows->windowFor(m_display.get());
    watchWindow(display, true);
    connect(m_windows.get(), &IWindowManager::windowClosed, this, [this](QObject *model) {
        // 辅窗关闭只影响诊断状态，不触发根窗关闭，也不自动重建。
        if (model == m_display.get()) m_displayClosed = true;
    });
    main->requestActivate();
    if (m_verifyRendering) QTimer::singleShot(1500, this, [this, main, display] {
        if (!main || !main->isVisible()) return;
        const bool mainReady = m_mainRendered || main->visibility() == QWindow::Minimized;
        const bool displayReady = m_displayClosed || (display && display->isExposed()
            && m_displayRendered && (!m_layout.fourScreen
                || (display->geometry() == m_layout.displayGeometry
                    && qFuzzyCompare(display->devicePixelRatio(), qreal(1)))));
        qInfo() << "Main window:" << main->geometry() << "visibility:" << main->visibility();
        if (display) qInfo() << "Display window:" << display->geometry() << "DPR:" << display->devicePixelRatio();
        if (!mainReady || !displayReady) {
            qCritical() << "Window geometry or first frame verification failed";
            QCoreApplication::exit(3);
        } else qInfo() << "Window verification passed; mode:" << (m_layout.fourScreen ? "four-screen" : "preview");
    });
    if (m_autoClose) QTimer::singleShot(m_autoClose * 1000, m_shell, [shell = m_shell] {
        if (shell) shell->exitApplication();
    });
    emit windowsReady();
}
}
