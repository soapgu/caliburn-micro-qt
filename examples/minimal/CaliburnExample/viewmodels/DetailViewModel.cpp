#include "DetailViewModel.h"
#include <QDebug>
#include <CaliburnMicroQt/ConfirmActionViewModel.h>
#include <stdexcept>
#include <utility>

DetailViewModel::DetailViewModel(std::shared_ptr<CounterService> counterService,
                               std::shared_ptr<IWindowManager> windowManager)
    : m_windowManager(std::move(windowManager)), m_counterService(std::move(counterService))
{
    if (!m_counterService)
        throw std::invalid_argument("Detail 要求非空的计数服务");
    if (!m_windowManager)
        throw std::invalid_argument("Detail 要求非空的窗口服务");
    connect(m_counterService.get(), &CounterService::countChanged, this, &DetailViewModel::countChanged);
}

DetailViewModel::~DetailViewModel()
{
    m_windowManager->cancelDialogsFor(this);
}

void DetailViewModel::goBack()
{
    if (!isActive()) return;
    try { tryClose(); }
    catch (const std::exception &error) { qWarning().noquote() << "Detail 返回失败：" << error.what(); }
    catch (...) { qWarning("Detail 返回失败：未知异常"); }
}

void DetailViewModel::canClose(CloseCallback callback)
{
    QFuture<DialogResult> result;
    try {
        auto dialog = std::make_unique<ConfirmActionViewModel>(
            ConfirmationRequest{QStringLiteral("离开详情"), QStringLiteral("确定离开当前详情吗？")},
            *m_windowManager);
        result = m_windowManager->showDialogAsync(std::move(dialog), this);
    } catch (...) {
        qWarning("Detail：退出确认失败");
        callback(false);
        return;
    }
    // 先转换窗口失败，再交付许可，避免关闭执行异常被误当成展示失败。
    result.onFailed(this, []() -> DialogResult {
        qWarning("Detail：退出确认失败");
        return false;
    }).then(this, [callback = std::move(callback)](DialogResult decision) {
        try { callback(decision.value_or(false)); }
        catch (const std::exception &error) { qWarning().noquote() << "Detail：关闭执行失败：" << error.what(); }
        catch (...) { qWarning("Detail：关闭执行失败：未知异常"); }
    });
}

void DetailViewModel::onDeactivate(bool close)
{
    Q_UNUSED(close);
    m_windowManager->cancelDialogsFor(this);
}
