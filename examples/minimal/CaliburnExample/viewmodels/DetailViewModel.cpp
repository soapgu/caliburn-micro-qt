#include "DetailViewModel.h"
#include <QDebug>
#include <stdexcept>
#include <utility>

DetailViewModel::DetailViewModel(std::shared_ptr<CounterService> counterService)
    : m_counterService(std::move(counterService))
{
    if (!m_counterService)
        throw std::invalid_argument("Detail 要求非空的计数服务");
    connect(m_counterService.get(), &CounterService::countChanged, this, &DetailViewModel::countChanged);
}

bool DetailViewModel::goBack()
{
    if (!isActive())
        return false;
    try {
        return tryClose();
    } catch (const std::exception &error) {
        qWarning().noquote() << "Detail 返回失败：" << error.what();
    } catch (...) {
        qWarning("Detail 返回失败：未知异常");
    }
    return false;
}
