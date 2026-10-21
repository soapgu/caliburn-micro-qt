#include "ViewModelComposition.h"
#include <boost/di.hpp>
#include <stdexcept>
#include <CaliburnMicroQt/WindowManager.h>

std::unique_ptr<ShellViewModel> buildShell()
{
    return buildShell(std::make_shared<WindowManager>());
}

std::unique_ptr<ShellViewModel> buildShell(std::shared_ptr<IWindowManager> windowManager)
{
    if (!windowManager)
        throw std::invalid_argument("Shell composition requires a non-null window service");
    auto counterService = std::make_shared<CounterService>();
    HomeViewModelFactory homeFactory = [counterService, windowManager] {
        auto injector = boost::di::make_injector(
            boost::di::bind<CounterService>.to(counterService),
            boost::di::bind<IWindowManager>.to(windowManager));
        return injector.create<std::unique_ptr<HomeViewModel>>();
    };
    DetailViewModelFactory detailFactory = [counterService, windowManager] {
        auto injector = boost::di::make_injector(
            boost::di::bind<CounterService>.to(counterService),
            boost::di::bind<IWindowManager>.to(windowManager));
        return injector.create<std::unique_ptr<DetailViewModel>>();
    };
    auto injector = boost::di::make_injector(
        boost::di::bind<HomeViewModelFactory>.to(homeFactory),
        boost::di::bind<DetailViewModelFactory>.to(detailFactory));
    return injector.create<std::unique_ptr<ShellViewModel>>();
}
