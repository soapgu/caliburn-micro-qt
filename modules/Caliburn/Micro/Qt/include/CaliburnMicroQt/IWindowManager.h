#pragma once

#include <CaliburnMicroQt/ScreenViewModel.h>
#include <QFuture>
#include <memory>
#include <optional>

using DialogResult = std::optional<bool>;

// Future 只表达弹窗结果；Screen 的同步生命周期不变。
class IWindowManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("窗口服务由框架创建")
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(ScreenViewModel *currentDialog READ currentDialog NOTIFY currentDialogChanged)
public:
    using QObject::QObject;
    virtual bool busy() const = 0;
    virtual ScreenViewModel *currentDialog() const = 0;
    virtual QFuture<DialogResult> showDialogAsync(std::unique_ptr<ScreenViewModel> viewModel,
                                                 QObject *requester) = 0;
    virtual void closeDialog(ScreenViewModel *viewModel, DialogResult result = std::nullopt) = 0;
    virtual void cancelDialogsFor(QObject *requester) = 0;
signals:
    void busyChanged();
    void currentDialogChanged();
};
