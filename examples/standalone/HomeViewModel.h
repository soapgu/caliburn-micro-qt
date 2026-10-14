#pragma once

#include <CaliburnMicroQt/ViewModelBase.h>

class HomeViewModel : public ViewModelBase
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by the consumer composition root")
    Q_PROPERTY(int count READ count NOTIFY countChanged)
public:
    using ViewModelBase::ViewModelBase;
    int count() const { return m_count; }
    Q_INVOKABLE void increment() { setAndNotify(m_count, m_count + 1, &HomeViewModel::countChanged); }
    void reset() { setAndNotify(m_count, 0, &HomeViewModel::countChanged); }
signals:
    void countChanged();
private:
    int m_count = 0;
};
