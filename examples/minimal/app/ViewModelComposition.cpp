#include "ViewModelComposition.h"
#include <boost/di.hpp>
#include <QQmlEngine>
#include <CaliburnMicroQt/WindowManager.h>

std::unique_ptr<ShellViewModel> buildShell()
{
    auto counterService = std::make_shared<CounterService>();
    std::shared_ptr<IWindowManager> windowManager = std::make_shared<WindowManager>();
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
        boost::di::bind<DetailViewModelFactory>.to(detailFactory),
        boost::di::bind<IWindowManager>.to(windowManager));
    auto shell = injector.create<std::unique_ptr<ShellViewModel>>();
    QQmlEngine::setObjectOwnership(shell.get(), QQmlEngine::CppOwnership);
    return shell;
}
