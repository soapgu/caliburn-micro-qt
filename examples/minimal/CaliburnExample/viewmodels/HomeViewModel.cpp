#include "HomeViewModel.h"
#include <CaliburnMicroQt/ConfirmActionViewModel.h>
#include <stdexcept>
#include <utility>

HomeViewModel::HomeViewModel(std::shared_ptr<CounterService> counterService,
                             std::shared_ptr<IWindowManager> windowManager)
    : m_counterService(std::move(counterService)), m_windowManager(std::move(windowManager))
{
    if (!m_counterService)
        throw std::invalid_argument("Home 要求非空的计数服务");
    if (!m_windowManager)
        throw std::invalid_argument("Home 要求非空的窗口服务");
    m_lastCanIncrement = canIncrement();
    m_lastCanAddTwo = canAddTwo();
    m_lastCanReset = canReset();
    connect(m_counterService.get(), &CounterService::countChanged,
            this, &HomeViewModel::notifyCountChanged);
}

QString HomeViewModel::message() const
{
    return QStringLiteral("已点击 %1 次").arg(count());
}

void HomeViewModel::increment()
{
    add(1);
}

void HomeViewModel::add(int delta)
{
    if (m_counterService->canAdd(delta))
        m_counterService->add(delta);
}

void HomeViewModel::reset()
{
    if (!canReset())
        return;
    const auto generation = ++m_resetGeneration;
    setResetPending(true);
    try {
        auto dialog = std::make_unique<ConfirmActionViewModel>(
            ConfirmationRequest{QStringLiteral("重置计数"), QStringLiteral("确定将计数重置为 0 吗？")},
            *m_windowManager);
        m_windowManager->showDialogAsync(std::move(dialog), this)
            .then(this, [this, generation](DialogResult result) {
                if (generation != m_resetGeneration)
                    return;
                if (result.value_or(false) && isActive())
                    m_counterService->reset();
                setResetPending(false);
            }).onFailed(this, [this, generation] {
                if (generation != m_resetGeneration)
                    return;
                setResetPending(false);
                qWarning("Home：重置确认失败");
            });
    } catch (...) {
        setResetPending(false);
        qWarning("Home：重置确认失败");
    }
}

void HomeViewModel::notifyCountChanged()
{
    const bool incrementChanged = m_lastCanIncrement != canIncrement();
    const bool addTwoChanged = m_lastCanAddTwo != canAddTwo();
    const bool resetChanged = m_lastCanReset != canReset();
    m_lastCanIncrement = canIncrement();
    m_lastCanAddTwo = canAddTwo();
    m_lastCanReset = canReset();
    emit countChanged();
    if (incrementChanged)
        emit canIncrementChanged();
    if (addTwoChanged)
        emit canAddTwoChanged();
    if (resetChanged)
        emit canResetChanged();
}

void HomeViewModel::setResetPending(bool pending)
{
    if (!setAndNotify(m_resetPending, pending, &HomeViewModel::resetPendingChanged))
        return;
    const bool available = canReset();
    if (m_lastCanReset != available) {
        m_lastCanReset = available;
        emit canResetChanged();
    }
}

void HomeViewModel::onDeactivate(bool close)
{
    Q_UNUSED(close);
    ++m_resetGeneration;
    m_windowManager->cancelDialogsFor(this);
    setResetPending(false);
}
