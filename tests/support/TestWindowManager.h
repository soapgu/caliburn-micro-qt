#pragma once

#include <CaliburnMicroQt/WindowManager.h>
#include <QPromise>
#include <stdexcept>

// 默认立即接受以保持旧业务测试的目的；异步测试可关闭自动完成。
class TestWindowManager : public WindowManager
{
public:
    bool autoComplete = true;
    bool deferCancellation = false;
    std::unique_ptr<QPromise<DialogResult>> deferred;
    DialogResult nextResult = true;
    int requests = 0;
    int cancellations = 0;
    bool busy() const override { return bool(m_promise); }
    ScreenViewModel *currentDialog() const override { return m_vm.get(); }
    QFuture<DialogResult> showDialogAsync(std::unique_ptr<ScreenViewModel> vm, QObject *requester) override
    {
        ++requests;
        m_vm = std::move(vm);
        m_requester = requester;
        m_promise = std::make_unique<QPromise<DialogResult>>();
        m_promise->start();
        auto future = m_promise->future();
        if (autoComplete)
            complete(nextResult);
        return future;
    }
    void closeDialog(ScreenViewModel *vm, DialogResult result = std::nullopt) override
    {
        if (vm == m_vm.get())
            complete(result);
    }
    void cancelDialogsFor(QObject *requester) override
    {
        if (requester == m_requester && m_promise) {
            ++cancellations;
            if (deferCancellation) {
                deferred = std::move(m_promise);
                m_vm.reset();
                m_requester = nullptr;
            } else {
                complete(std::nullopt);
            }
        }
    }
    void complete(DialogResult result)
    {
        if (!m_promise)
            return;
        auto promise = std::move(m_promise);
        m_vm.reset();
        m_requester = nullptr;
        promise->addResult(result);
        promise->finish();
    }
    void fail()
    {
        auto promise = std::move(m_promise);
        m_vm.reset();
        m_requester = nullptr;
        promise->setException(std::make_exception_ptr(std::runtime_error("测试展示失败")));
        promise->finish();
    }
private:
    std::unique_ptr<QPromise<DialogResult>> m_promise;
    std::unique_ptr<ScreenViewModel> m_vm;
    QObject *m_requester = nullptr;
};

inline std::shared_ptr<IWindowManager> testWindows()
{
    return std::make_shared<TestWindowManager>();
}
