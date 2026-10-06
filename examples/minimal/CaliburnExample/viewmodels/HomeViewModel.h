#pragma once

#include <CaliburnMicroQt/ScreenViewModel.h>
#include <QString>

class HomeViewModel : public ScreenViewModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("由示例应用装配层创建")
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QString message READ message NOTIFY countChanged)
    Q_PROPERTY(QString incrementText READ incrementText CONSTANT)
    Q_PROPERTY(bool canIncrement READ canIncrement NOTIFY canIncrementChanged)
    Q_PROPERTY(bool canAddTwo READ canAddTwo NOTIFY canAddTwoChanged)
    Q_PROPERTY(bool canReset READ canReset NOTIFY canResetChanged)

public:
    HomeViewModel();
    int count() const { return m_count; }
    QString message() const;
    QString incrementText() const { return QStringLiteral("增加"); }
    bool canIncrement() const { return canAdd(1); }
    bool canAddTwo() const { return canAdd(2); }
    bool canReset() const { return m_count > 0; }
    Q_INVOKABLE void increment();
    Q_INVOKABLE void add(int delta);
    Q_INVOKABLE void reset();

signals:
    void countChanged();
    void canIncrementChanged();
    void canAddTwoChanged();
    void canResetChanged();

private:
    bool canAdd(int delta) const { return delta > 0 && delta <= 5 - m_count; }
    void updateCount(int value);
    int m_count = 0;
};
