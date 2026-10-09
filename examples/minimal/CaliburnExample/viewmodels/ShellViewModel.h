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
    Q_PROPERTY(IWindowManager *windowManager READ windowManager CONSTANT)
    Q_PROPERTY(HomeViewModel *home READ home NOTIFY itemsChanged)
    Q_PROPERTY(DetailViewModel *detail READ detail NOTIFY itemsChanged)
    Q_PROPERTY(bool canShowDetail READ canShowDetail NOTIFY navigationChanged)
public:
    explicit ShellViewModel(HomeViewModelFactory homeFactory, DetailViewModelFactory detailFactory,
                            std::shared_ptr<IWindowManager> windowManager);
    IWindowManager *windowManager() const { return m_windowManager.get(); }
    HomeViewModel *home() const;
    DetailViewModel *detail() const;
    bool canShowDetail() const { return isActive() && home() && activeItem() == home(); }
    Q_INVOKABLE bool showDetail();
    bool deactivateItem(ViewModelBase *item, bool close) override;
signals:
    void navigationChanged();
protected:
    void onActivate() override;
private:
    HomeViewModel *ensureHome();
    std::shared_ptr<IWindowManager> m_windowManager;
    HomeViewModelFactory m_homeFactory;
    DetailViewModelFactory m_detailFactory;
};
