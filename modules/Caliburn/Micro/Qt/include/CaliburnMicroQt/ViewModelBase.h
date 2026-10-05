#pragma once

#include <QObject>
#include <QDebug>
#include <QtQmlIntegration/qqmlintegration.h>
#include <type_traits>

class ViewModelBase : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("ViewModel 由 C++ 应用装配层创建")

public:
    explicit ViewModelBase(QObject *parent = nullptr);
    ~ViewModelBase() override = default;

protected:
    template<class Owner, class T>
    bool setAndNotify(T &field, const T &value, void (Owner::*notifySignal)())
    {
        static_assert(std::is_base_of_v<ViewModelBase, Owner>,
                      "Owner 必须继承 ViewModelBase");
        auto *owner = qobject_cast<Owner *>(this);
        if (!notifySignal || !owner) {
            qWarning("ViewModelBase::setAndNotify: 信号为空或对象类型不兼容");
            return false;
        }
        if (field == value)
            return false;
        field = value;
        (owner->*notifySignal)();
        return true;
    }
};
