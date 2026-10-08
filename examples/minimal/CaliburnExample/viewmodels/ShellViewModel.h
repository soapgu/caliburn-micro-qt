#pragma once

#include <CaliburnMicroQt/Conductor.h>
#include <HomeViewModel.h>
#include <DetailViewModel.h>
#include <functional>
#include <memory>

using HomeViewModelFactory = std::function<std::unique_ptr<HomeViewModel>()>;
using DetailViewModelFactory = std::function<std::unique_ptr<DetailViewModel>()>;

// 模板层没有自己的元对象；moc/QML 工具使用其实际元对象基类。
#ifdef Q_MOC_RUN
class ShellViewModel : public ConductorCollectionOneActiveViewModelBase
#else
class ShellViewModel : public Conductor<ScreenViewModel>::Collection::OneActive
#endif
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("由示例应用装配层创建")
    Q_PROPERTY(HomeViewModel *home READ home NOTIFY itemsChanged)
    Q_PROPERTY(DetailViewModel *detail READ detail NOTIFY itemsChanged)
    Q_PROPERTY(bool canShowDetail READ canShowDetail NOTIFY navigationChanged)
    Q_PROPERTY(bool canGoHome READ canGoHome NOTIFY navigationChanged)
public:
    explicit ShellViewModel(HomeViewModelFactory homeFactory, DetailViewModelFactory detailFactory);
    HomeViewModel *home() const;
    DetailViewModel *detail() const;
    bool canShowDetail() const { return isActive() && home() && activeItem() == home(); }
    bool canGoHome() const { return isActive() && detail() && activeItem() == detail(); }
    Q_INVOKABLE bool showDetail();
    Q_INVOKABLE bool goHome();
signals:
    void navigationChanged();
protected:
    void onActivate() override;
private:
    HomeViewModel *ensureHome();
    HomeViewModelFactory m_homeFactory;
    DetailViewModelFactory m_detailFactory;
};
