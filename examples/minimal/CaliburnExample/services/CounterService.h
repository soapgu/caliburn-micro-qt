#pragma once

#include <QObject>

// 示例业务事实与范围约束；不向 QML 注册，不使用 QObject 父所有权。
class CounterService : public QObject
{
    Q_OBJECT
public:
    CounterService() = default;
    int count() const { return m_count; }
    bool canAdd(int delta) const { return delta > 0 && delta <= 5 - m_count; }
    void add(int delta);
    void reset();

signals:
    void countChanged();

private:
    int m_count = 0;
};
