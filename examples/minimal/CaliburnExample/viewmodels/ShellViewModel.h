#pragma once

#include <CaliburnMicroQt/Conductor.h>
#include <HomeViewModel.h>
#include <functional>
#include <memory>

using HomeViewModelFactory = std::function<std::unique_ptr<HomeViewModel>()>;

// 模板层没有自己的元对象；moc/QML 工具使用其实际元对象基类。
#ifdef Q_MOC_RUN
class ShellViewModel : public ConductorViewModelBase
#else
class ShellViewModel : public Conductor<ScreenViewModel>
#endif
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("由示例应用装配层创建")
    Q_PROPERTY(HomeViewModel *home READ home NOTIFY activeItemChanged)

public:
    explicit ShellViewModel(HomeViewModelFactory homeFactory);
    HomeViewModel *home() const { return qobject_cast<HomeViewModel *>(activeItem()); }

protected:
    void onActivate() override;

private:
    void createHome();
    HomeViewModelFactory m_homeFactory;
};
