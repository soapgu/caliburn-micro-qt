#pragma once

#include <CaliburnMicroQt/ViewModelBase.h>
#include <CaliburnMicroQt/IChild.h>
#include <QPointer>
#include <CaliburnMicroQt/IGuardClose.h>

class ScreenViewModel : public ViewModelBase, public IChild, public IGuardClose
{
    Q_OBJECT
    Q_INTERFACES(IChild IGuardClose)
    QML_ELEMENT
    QML_UNCREATABLE("Screen 由 C++ 应用装配层创建")
    Q_PROPERTY(QObject *parentViewModel READ parentViewModel NOTIFY parentViewModelChanged)
    Q_PROPERTY(bool isInitialized READ isInitialized NOTIFY isInitializedChanged)
    Q_PROPERTY(bool isActive READ isActive NOTIFY isActiveChanged)

public:
    explicit ScreenViewModel(QObject *parent = nullptr);
    QObject *parentViewModel() const override { return m_parentViewModel.data(); }
    bool isInitialized() const { return m_initialized; }
    bool isActive() const { return m_active; }
    // 先提交状态并通知，再执行钩子；异常不回滚，回调不得重入生命周期。
    void initialize();
    void activate();
    void deactivate(bool close = false);
    // 请求方法返回不代表已经关闭；无逻辑 Parent 时无操作。
    void tryClose();
    void canClose(CloseCallback callback) override;

signals:
    void attemptingDeactivation(bool close);
    void deactivated(bool close);
    void parentViewModelChanged();
    void isInitializedChanged();
    void isActiveChanged();

protected:
    virtual void onInitialize() {}
    virtual void onActivate() {}
    virtual void onDeactivate(bool close) { Q_UNUSED(close); }

private:
    void setParentViewModel(QObject *parent) override;
    QPointer<QObject> m_parentViewModel;
    bool m_initialized = false;
    bool m_active = false;
};
