#pragma once

#include <QObject>

class ConductorBase;

class IChild
{
public:
    virtual ~IChild() = default;
    virtual QObject *parentViewModel() const = 0;

protected:
    friend class ConductorBase;
    virtual void setParentViewModel(QObject *parent) = 0;
};

Q_DECLARE_INTERFACE(IChild, "Caliburn.Micro.Qt.IChild/1.0")
