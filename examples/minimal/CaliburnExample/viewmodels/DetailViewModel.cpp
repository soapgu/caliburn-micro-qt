#include "DetailViewModel.h"
#include <stdexcept>
#include <utility>

DetailViewModel::DetailViewModel(std::shared_ptr<CounterService> counterService)
    : m_counterService(std::move(counterService))
{
    if (!m_counterService)
        throw std::invalid_argument("Detail 要求非空的计数服务");
    connect(m_counterService.get(), &CounterService::countChanged, this, &DetailViewModel::countChanged);
}
