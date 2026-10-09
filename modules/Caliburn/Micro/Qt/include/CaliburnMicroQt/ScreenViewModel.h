#pragma once

#include <CaliburnMicroQt/ViewModelBase.h>
#include <CaliburnMicroQt/IChild.h>
#include <QPointer>

class ScreenViewModel : public ViewModelBase, public IChild
{
    Q_OBJECT
    Q_INTERFACES(IChild)
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
    // 向逻辑 Parent 请求关闭；成功表示已处理请求，实际回收可以延迟。
    bool tryClose();

signals:
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
