#include "ViewModelComposition.h"
#include <boost/di.hpp>
#include <QQmlEngine>

std::unique_ptr<ShellViewModel> buildShell()
{
    auto counterService = std::make_shared<CounterService>();
    HomeViewModelFactory homeFactory = [counterService] {
        auto injector = boost::di::make_injector(
            boost::di::bind<CounterService>.to(counterService));
        return injector.create<std::unique_ptr<HomeViewModel>>();
    };
    auto injector = boost::di::make_injector(
        boost::di::bind<HomeViewModelFactory>.to(homeFactory));
    auto shell = injector.create<std::unique_ptr<ShellViewModel>>();
    QQmlEngine::setObjectOwnership(shell.get(), QQmlEngine::CppOwnership);
    return shell;
}
