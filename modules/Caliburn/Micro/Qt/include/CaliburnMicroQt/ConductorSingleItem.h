#pragma once

#include <CaliburnMicroQt/ConductorBaseWithActiveItem.h>
#include <QPointer>
#include <memory>

class ConductorSingleItem : public ConductorBaseWithActiveItem
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Conductor is created by the C++ composition layer")

public:
    explicit ConductorSingleItem(QObject *parent = nullptr);
    QList<ViewModelBase *> getChildren() const override;
    void activateItem(ViewModelBase *item) override;
    void deactivateItem(ViewModelBase *item, bool close) override;
    void canClose(CloseCallback callback) override;

protected:
    bool validateItemChange(ViewModelBase *item) const;
    void adoptAndActivate(std::unique_ptr<ViewModelBase> item);
    void onActivate() override;
    void onDeactivate(bool close) override;

};
