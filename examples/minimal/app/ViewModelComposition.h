#pragma once

#include <ShellViewModel.h>
#include <memory>

// 只装配对象和所有权；初始化、激活由 Bootstrapper 显式调用。
std::unique_ptr<ShellViewModel> buildShell();
