#pragma once

#include <ShellViewModel.h>
#include <memory>

// 只装配对象和所有权；初始化、激活由 Bootstrapper 显式调用。
std::unique_ptr<ShellViewModel> buildShell();

// 使用 Bootstrapper 提供的同一窗口服务装配业务 VM。
std::unique_ptr<ShellViewModel> buildShell(std::shared_ptr<IWindowManager> windowManager);
