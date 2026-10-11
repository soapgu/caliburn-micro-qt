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
        if (m_promise) {
            QPromise<DialogResult> rejected;
            rejected.start();
            auto result = rejected.future();
            rejected.setException(std::make_exception_ptr(std::runtime_error("测试窗口忙")));
            rejected.finish();
            return result;
        }
        m_vm = std::move(vm);
        m_promise = std::make_unique<QPromise<DialogResult>>();
        m_promise->start();
        auto future = m_promise->future();
        connect(m_vm.get(), &ScreenViewModel::closeRequested, this, [this](DialogResult result) {
            if (!result) ++cancellations;
            if (!result && deferCancellation) {
                disconnect(m_requesterDestroyed);
                deferred = std::move(m_promise);
                m_vm.reset();
            } else {
                complete(result);
            }
        });
        m_requesterDestroyed = connect(requester, &QObject::destroyed, this, [this] {
            ++cancellations;
            complete(std::nullopt);
        });
        if (autoComplete)
            complete(nextResult);
        return future;
    }
    void complete(DialogResult result)
    {
        if (!m_promise)
            return;
        disconnect(m_requesterDestroyed);
        auto promise = std::move(m_promise);
        m_vm.reset();
        promise->addResult(result);
        promise->finish();
    }
    void fail()
    {
        disconnect(m_requesterDestroyed);
        auto promise = std::move(m_promise);
        m_vm.reset();
        promise->setException(std::make_exception_ptr(std::runtime_error("测试展示失败")));
        promise->finish();
    }
private:
    std::unique_ptr<QPromise<DialogResult>> m_promise;
    std::unique_ptr<ScreenViewModel> m_vm;
    QMetaObject::Connection m_requesterDestroyed;
};

inline std::shared_ptr<IWindowManager> testWindows()
{
    return std::make_shared<TestWindowManager>();
}
