#include "ViewModelComposition.h"
#include <boost/di.hpp>
#include <QQmlEngine>

std::unique_ptr<ShellViewModel> buildShell()
{
    auto injector = boost::di::make_injector();
    auto shell = injector.create<std::unique_ptr<ShellViewModel>>();
    QQmlEngine::setObjectOwnership(shell.get(), QQmlEngine::CppOwnership);
    QQmlEngine::setObjectOwnership(shell->home(), QQmlEngine::CppOwnership);
    return shell;
}
