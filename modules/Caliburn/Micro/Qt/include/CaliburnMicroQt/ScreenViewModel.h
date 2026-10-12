#pragma once

#include <CaliburnMicroQt/ViewModelBase.h>
#include <CaliburnMicroQt/IChild.h>
#include <CaliburnMicroQt/IActivate.h>
#include <CaliburnMicroQt/IDeactivate.h>
#include <QPointer>
#include <CaliburnMicroQt/IGuardClose.h>
#include <optional>

using DialogResult = std::optional<bool>;

class ScreenViewModel : public ViewModelBase, public IChild, public IGuardClose,
                        public IActivate, public IDeactivate
{
    Q_OBJECT
    Q_INTERFACES(IChild IGuardClose IActivate IDeactivate)
    QML_ELEMENT
    QML_UNCREATABLE("Screen is created by the C++ composition layer")
    Q_PROPERTY(QObject *parentViewModel READ parentViewModel NOTIFY parentViewModelChanged)
    Q_PROPERTY(bool isInitialized READ isInitialized NOTIFY isInitializedChanged)
    Q_PROPERTY(bool isActive READ isActive NOTIFY isActiveChanged)

public:
    explicit ScreenViewModel(QObject *parent = nullptr);
    QObject *parentViewModel() const override { return m_parentViewModel.data(); }
    bool isInitialized() const { return m_initialized; }
    bool isActive() const override { return m_active; }
    // 先提交状态并通知，再执行钩子；异常不回滚，回调不得重入生命周期。
    void initialize();
    void activate() override;
    void deactivate(bool close = false) override;
    // 请求方法返回不代表已经关闭；无逻辑 Parent 时请求关联窗口关闭。
    void tryClose(DialogResult dialogResult = std::nullopt);
    void canClose(CloseCallback callback) override;

signals:
    void closeRequested(DialogResult dialogResult);
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
