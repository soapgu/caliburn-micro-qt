#pragma once

#include <QList>
#include <QObject>

class ViewModelBase;

class IParent
{
public:
    virtual ~IParent() = default;
    virtual QList<ViewModelBase *> getChildren() const = 0;
};

Q_DECLARE_INTERFACE(IParent, "Caliburn.Micro.Qt.IParent/1.0")
