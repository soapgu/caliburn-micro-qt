#include "ViewModelComposition.h"
#include <boost/di.hpp>
#include <stdexcept>
#include <QQmlEngine>
#include <CaliburnMicroQt/WindowManager.h>

std::unique_ptr<ShellViewModel> buildShell()
{
    return buildShell(std::make_shared<WindowManager>());
}

std::unique_ptr<ShellViewModel> buildShell(std::shared_ptr<IWindowManager> windowManager)
{
    if (!windowManager)
        throw std::invalid_argument("装配 Shell 要求非空的窗口服务");
    auto counterService = std::make_shared<CounterService>();
    QQmlEngine::setObjectOwnership(windowManager.get(), QQmlEngine::CppOwnership);
    HomeViewModelFactory homeFactory = [counterService, windowManager] {
        auto injector = boost::di::make_injector(
            boost::di::bind<CounterService>.to(counterService),
            boost::di::bind<IWindowManager>.to(windowManager));
        return injector.create<std::unique_ptr<HomeViewModel>>();
    };
    DetailViewModelFactory detailFactory = [counterService] {
        auto injector = boost::di::make_injector(
            boost::di::bind<CounterService>.to(counterService));
        return injector.create<std::unique_ptr<DetailViewModel>>();
    };
    auto injector = boost::di::make_injector(
        boost::di::bind<HomeViewModelFactory>.to(homeFactory),
        boost::di::bind<DetailViewModelFactory>.to(detailFactory));
    auto shell = injector.create<std::unique_ptr<ShellViewModel>>();
    QQmlEngine::setObjectOwnership(shell.get(), QQmlEngine::CppOwnership);
    return shell;
}
