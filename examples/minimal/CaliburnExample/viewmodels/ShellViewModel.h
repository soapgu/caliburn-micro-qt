#pragma once

#include <CaliburnMicroQt/ViewModelBase.h>
#include <QString>

class ShellViewModel : public ViewModelBase
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("由示例应用装配层创建")
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QString message READ message NOTIFY countChanged)
    Q_PROPERTY(QString incrementText READ incrementText CONSTANT)
    Q_PROPERTY(bool canIncrement READ canIncrement NOTIFY canIncrementChanged)
    Q_PROPERTY(bool canReset READ canReset NOTIFY canResetChanged)

public:
    explicit ShellViewModel(QObject *parent = nullptr);
    int count() const { return m_count; }
    QString message() const;
    QString incrementText() const { return QStringLiteral("增加"); }
    bool canIncrement() const { return m_count < 5; }
    bool canReset() const { return m_count > 0; }
    Q_INVOKABLE void increment();
    Q_INVOKABLE void reset();

signals:
    void countChanged();
    void canIncrementChanged();
    void canResetChanged();

private:
    void updateCount(int value);
    int m_count = 0;
};
