#include "ShellViewModel.h"
#include <CaliburnMicroQt/ConfirmActionViewModel.h>
#include <stdexcept>
#include <utility>

ShellViewModel::ShellViewModel(std::shared_ptr<IWindowManager> windows)
    : m_windows(std::move(windows))
{
    if (!m_windows)
        throw std::invalid_argument("Consumer requires a window service");
}

void ShellViewModel::requestReset()
{
    if (m_resetPending || m_home.count() == 0)
        return;
    setAndNotify(m_resetPending, true, &ShellViewModel::resetPendingChanged);
    auto confirmation = std::make_unique<ConfirmActionViewModel>(ConfirmationRequest{
        QStringLiteral("独立消费工程"), QStringLiteral("确认重置计数？")});
    m_windows->showDialogAsync(std::move(confirmation), this)
        .then(this, [this](DialogResult result) {
            if (result.value_or(false) && isActive())
                m_home.reset();
            setAndNotify(m_resetPending, false, &ShellViewModel::resetPendingChanged);
        }).onFailed(this, [this] {
            setAndNotify(m_resetPending, false, &ShellViewModel::resetPendingChanged);
            qWarning("Consumer: reset confirmation failed");
        });
}
