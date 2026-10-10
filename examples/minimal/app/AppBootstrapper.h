#pragma once

#include <CaliburnMicroQt/BootstrapperBase.h>

class AppBootstrapper final : public BootstrapperBase
{
public:
    using BootstrapperBase::BootstrapperBase;

protected:
    bool Configure() override;
    void OnStartup() override;
};
