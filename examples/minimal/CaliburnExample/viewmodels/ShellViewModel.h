#pragma once

#include <CaliburnMicroQt/ScreenViewModel.h>
#include <HomeViewModel.h>
#include <QPointer>
#include <memory>

class ShellViewModel : public ScreenViewModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("由示例应用装配层创建")
    Q_PROPERTY(HomeViewModel *home READ home NOTIFY homeChanged)

public:
    explicit ShellViewModel(std::unique_ptr<HomeViewModel> home);
    HomeViewModel *home() const { return m_home.data(); }

signals:
    void homeChanged();

protected:
    void onInitialize() override;
    void onActivate() override;
    void onDeactivate(bool close) override;

private:
    QPointer<HomeViewModel> m_home;
};
