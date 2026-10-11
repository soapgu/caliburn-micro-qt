#pragma once

#include <CaliburnMicroQt/IWindowManager.h>
#include <QPointer>
#include <QPromise>

class QQmlEngine;
class QQuickWindow;
class QQuickItem;
class WindowManager : public IWindowManager
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("窗口服务由框架创建")
public:
    explicit WindowManager(QObject *parent = nullptr);
    ~WindowManager() override;
    [[nodiscard]] bool showWindow(const QVariant &viewModel) override;
    // 停用普通窗口桥接并结束弹窗；保留普通 View 供应用退出钩子使用。
    void prepareForShutdown();
    // 释放自建窗口/引擎并兜底关闭借用 VM；不删除 VM。
    void releaseWindows();
    bool busy() const override;
    ScreenViewModel *currentDialog() const override;
    QFuture<DialogResult> showDialogAsync(std::unique_ptr<ScreenViewModel> viewModel,
                                         QObject *requester) override;
signals:
    // 普通窗口关闭生命周期异常已捕获；应用可据此保留失败退出码。
    void windowCleanupFailed();
private:
    // 登记独立模态窗口的所属窗口、引擎和后备焦点，不修改业务内容树。
    [[nodiscard]] bool attachToWindow(QQuickWindow *window, QQuickItem *fallbackFocusItem = nullptr);
    // 强制结束当前弹窗并解除所属窗口关联。
    void detachFromWindow();
    struct ManagedWindow;
    std::unique_ptr<ManagedWindow> m_managedWindow;
    void closeManagedWindow() noexcept;
    struct Request;
    void complete(DialogResult result, std::exception_ptr error = {});
    void finish(bool restoreFocus = false);
    QPointer<QQuickWindow> m_window;
    QPointer<QQmlEngine> m_engine; // 借用所属窗口引擎；自建引擎归 ManagedWindow。
    QPointer<QQuickItem> m_fallbackFocus;
    QMetaObject::Connection m_engineDestroyed;
    QMetaObject::Connection m_windowDestroyed;
    std::unique_ptr<Request> m_request;
    bool m_finishing = false;
    bool m_starting = false;
    bool m_destroying = false;
};
