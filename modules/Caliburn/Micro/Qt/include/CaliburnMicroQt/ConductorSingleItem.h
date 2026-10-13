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
    QList<QObject *> getChildren() const override;
    void activateItem(QObject *item) override;
    void deactivateItem(QObject *item, bool close) override;
    void canClose(CloseCallback callback) override;

protected:
    bool validateItemChange(QObject *item) const;
    void adoptAndActivate(std::unique_ptr<QObject> item);
    void onActivate() override;
    void onDeactivate(bool close) override;

};
