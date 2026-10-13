#pragma once

#include <QList>
#include <QObject>

class IParent
{
public:
    virtual ~IParent() = default;
    virtual QList<QObject *> getChildren() const = 0;
};

Q_DECLARE_INTERFACE(IParent, "Caliburn.Micro.Qt.IParent/2.0")
