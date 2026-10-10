#pragma once

#include <CaliburnMicroQt/ScreenViewModel.h>
#include <CounterService.h>
#include <QString>
#include <memory>
#include <CaliburnMicroQt/IWindowManager.h>

class DetailViewModel : public ScreenViewModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("由示例应用装配层创建")
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QString message READ message NOTIFY countChanged)
public:
    explicit DetailViewModel(std::shared_ptr<CounterService> counterService,
                             std::shared_ptr<IWindowManager> windowManager);
    ~DetailViewModel() override;
    Q_INVOKABLE void goBack();
    void canClose(CloseCallback callback) override;
    int count() const { return m_counterService->count(); }
    QString message() const { return QStringLiteral("共享计数：%1").arg(count()); }
signals:
    void countChanged();
protected:
    void onDeactivate(bool close) override;
private:
    std::shared_ptr<IWindowManager> m_windowManager;
    std::shared_ptr<CounterService> m_counterService;
};
