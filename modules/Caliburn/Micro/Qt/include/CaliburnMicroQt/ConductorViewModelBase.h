#pragma once

#include <CaliburnMicroQt/ScreenViewModel.h>
#include <QPointer>
#include <memory>

class ConductorViewModelBase : public ScreenViewModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Conductor 由 C++ 应用装配层创建")
    Q_PROPERTY(ViewModelBase *activeItem READ activeItem NOTIFY activeItemChanged)

public:
    explicit ConductorViewModelBase(QObject *parent = nullptr);
    ~ConductorViewModelBase() override;
    ViewModelBase *activeItem() const { return m_activeItem.data(); }

signals:
    void activeItemChanged();

protected:
    bool validateItemChange(ViewModelBase *item) const;
    void changeActiveItem(std::unique_ptr<ViewModelBase> item);
    bool closeCurrentItem(ViewModelBase *item);
    void onActivate() override;
    void onDeactivate(bool close) override;

private:
    QPointer<ViewModelBase> m_activeItem;
    QMetaObject::Connection m_destroyed;
};
