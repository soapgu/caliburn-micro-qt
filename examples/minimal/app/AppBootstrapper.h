#pragma once

#include <CaliburnMicroQt/BootstrapperBase.h>

class AppBootstrapper final : public BootstrapperBase
{
public:
    using BootstrapperBase::BootstrapperBase;

protected:
    bool configure() override;
    void onStartup() override;
};
