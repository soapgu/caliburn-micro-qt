#pragma once

#include <CaliburnMicroQt/ScreenViewModel.h>
#include <CounterService.h>
#include <QString>
#include <memory>

class DetailViewModel : public ScreenViewModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("由示例应用装配层创建")
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QString message READ message NOTIFY countChanged)
public:
    explicit DetailViewModel(std::shared_ptr<CounterService> counterService);
    int count() const { return m_counterService->count(); }
    QString message() const { return QStringLiteral("共享计数：%1").arg(count()); }
signals:
    void countChanged();
private:
    std::shared_ptr<CounterService> m_counterService;
};
