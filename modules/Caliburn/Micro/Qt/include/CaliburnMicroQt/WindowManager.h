#pragma once

#include <CaliburnMicroQt/IWindowManager.h>
#include <QPointer>
#include <QPromise>

class DialogHostState;
class WindowManager : public IWindowManager
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("窗口服务由应用装配层创建")
public:
    explicit WindowManager(QObject *parent = nullptr);
    ~WindowManager() override;
    bool busy() const override;
    ScreenViewModel *currentDialog() const override;
    QFuture<DialogResult> showDialogAsync(std::unique_ptr<ScreenViewModel> viewModel,
                                         QObject *requester) override;
    void closeDialog(ScreenViewModel *viewModel, DialogResult result = std::nullopt) override;
    void cancelDialogsFor(QObject *requester) override;
private:
    friend class DialogHostState;
    struct Request;
    bool attachHost(DialogHostState *host);
    void detachHost(DialogHostState *host);
    void fail(const QString &id, const QString &message);
    void release(const QString &id);
    void complete(DialogResult result, std::exception_ptr error = {});
    void finish();
    QString requestId() const;
    QPointer<DialogHostState> m_host;
    std::unique_ptr<Request> m_request;
    quint64 m_nextId = 0;
    bool m_finishing = false;
    bool m_starting = false;
    bool m_destroying = false;
};
