#pragma once

#include <CaliburnMicroQt/ConfirmationRequest.h>
#include <CaliburnMicroQt/ScreenViewModel.h>

class ConfirmActionViewModel : public ScreenViewModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("确认 VM 由 C++ 创建")
    Q_PROPERTY(QString title READ title CONSTANT)
    Q_PROPERTY(QString message READ message CONSTANT)
    Q_PROPERTY(QString confirmText READ confirmText CONSTANT)
    Q_PROPERTY(QString cancelText READ cancelText CONSTANT)
public:
    explicit ConfirmActionViewModel(ConfirmationRequest request);
    QString title() const { return m_request.title; }
    QString message() const { return m_request.message; }
    QString confirmText() const { return m_request.confirmText; }
    QString cancelText() const { return m_request.cancelText; }
    Q_INVOKABLE void accept();
    Q_INVOKABLE void cancel();
private:
    ConfirmationRequest m_request;
};
