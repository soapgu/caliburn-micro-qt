#pragma once

#include <CaliburnMicroQt/DialogResult.h>
#include <QObject>
#include <QtQmlIntegration/qqmlintegration.h>
#include <QFuture>
#include <QVariant>
#include <memory>

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
    [[nodiscard]] virtual bool showWindow(const QVariant &viewModel) = 0;
    virtual bool busy() const = 0;
    virtual QObject *currentDialog() const = 0;
    virtual QFuture<DialogResult> showDialogAsync(std::unique_ptr<QObject> viewModel,
                                                 QObject *requester) = 0;
signals:
    void busyChanged();
    void currentDialogChanged();
};
