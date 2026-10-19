#pragma once

#include <CaliburnMicroQt/DialogResult.h>
#include <CaliburnMicroQt/DialogOptions.h>
#include <CaliburnMicroQt/WindowOptions.h>
#include <QObject>
#include <QtQmlIntegration/qqmlintegration.h>
#include <QFuture>
#include <QVariant>
#include <memory>

class QQuickWindow;

// Future 只表达弹窗结果；可选接口的同步生命周期不变。
class IWindowManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Window service is created by the framework")
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QObject *currentDialog READ currentDialog NOTIFY currentDialogChanged)
public:
    using QObject::QObject;
    // 借用普通窗口 VM；保留 QVariant 中的具体指针类型供 QML 初始属性注入。
    [[nodiscard]] virtual bool showWindow(const QVariant &viewModel,
                                          const WindowOptions &options = {}) = 0;
    // 借用指针，仅供启动层诊断；资源释放后失效，调用者应使用 QPointer。
    virtual QQuickWindow *windowFor(QObject *viewModel) const = 0;
    virtual bool busy() const = 0;
    virtual QObject *currentDialog() const = 0;
    virtual QFuture<DialogResult> showDialogAsync(std::unique_ptr<QObject> viewModel,
                                                 QObject *requester,
                                                 const DialogOptions &options = {}) = 0;
signals:
    // 参数是已关闭窗口对应的 VM，可能在关闭生命周期中销毁而为空。
    void windowClosed(QObject *viewModel);
    void busyChanged();
    void currentDialogChanged();
};
