#include <CaliburnMicroQt/ConfirmActionViewModel.h>
#include <utility>

ConfirmActionViewModel::ConfirmActionViewModel(ConfirmationRequest request, IWindowManager &manager)
    : m_request(std::move(request)), m_manager(&manager) {}

void ConfirmActionViewModel::accept()
{
    if (m_manager)
        m_manager->closeDialog(this, true);
}

void ConfirmActionViewModel::cancel()
{
    if (m_manager)
        m_manager->closeDialog(this, false);
}
