#pragma once

#include <CaliburnMicroQt/ScreenViewModel.h>
#include <CounterService.h>
#include <CaliburnMicroQt/IWindowManager.h>
#include <QString>
#include <memory>

class HomeViewModel : public ScreenViewModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("由示例应用装配层创建")
    Q_PROPERTY(bool resetPending READ resetPending NOTIFY resetPendingChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QString message READ message NOTIFY countChanged)
    Q_PROPERTY(QString incrementText READ incrementText CONSTANT)
    Q_PROPERTY(bool canIncrement READ canIncrement NOTIFY canIncrementChanged)
    Q_PROPERTY(bool canAddTwo READ canAddTwo NOTIFY canAddTwoChanged)
    Q_PROPERTY(bool canReset READ canReset NOTIFY canResetChanged)

public:
    explicit HomeViewModel(std::shared_ptr<CounterService> counterService,
                           std::shared_ptr<IWindowManager> windowManager);
    int count() const { return m_counterService->count(); }
    QString message() const;
    QString incrementText() const { return QStringLiteral("增加"); }
    bool canIncrement() const { return m_counterService->canAdd(1); }
    bool canAddTwo() const { return m_counterService->canAdd(2); }
    bool canReset() const { return count() > 0 && !m_resetPending; }
    bool resetPending() const { return m_resetPending; }
    Q_INVOKABLE void increment();
    Q_INVOKABLE void add(int delta);
    Q_INVOKABLE void reset();

signals:
    void resetPendingChanged();
    void countChanged();
    void canIncrementChanged();
    void canAddTwoChanged();
    void canResetChanged();

protected:
    void onDeactivate(bool close) override;
private:
    void setResetPending(bool pending);
    void notifyCountChanged();
    std::shared_ptr<CounterService> m_counterService;
    std::shared_ptr<IWindowManager> m_windowManager;
    bool m_resetPending = false;
    quint64 m_resetGeneration = 0;
    bool m_lastCanIncrement;
    bool m_lastCanAddTwo;
    bool m_lastCanReset;
};
