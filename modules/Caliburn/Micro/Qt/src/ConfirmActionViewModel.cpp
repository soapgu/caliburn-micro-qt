#include <CaliburnMicroQt/ConfirmActionViewModel.h>
#include <utility>

ConfirmActionViewModel::ConfirmActionViewModel(ConfirmationRequest request)
    : m_request(std::move(request)) {}

void ConfirmActionViewModel::accept()
{
    tryClose(true);
}

void ConfirmActionViewModel::cancel()
{
    tryClose(false);
}
