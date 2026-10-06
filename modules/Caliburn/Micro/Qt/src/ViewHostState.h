#pragma once

#include <CaliburnMicroQt/ViewModelBase.h>
#include <QPointer>

// 宿主内部的借用保护；应用应使用 ViewHost，不直接依赖此辅助类型。
class ViewHostState : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(ViewModelBase *model READ model WRITE setModel NOTIFY modelChanged)

public:
    explicit ViewHostState(QObject *parent = nullptr) : QObject(parent) {}
    ViewModelBase *model() const { return m_model.data(); }
    void setModel(ViewModelBase *model);
    Q_INVOKABLE bool matchesView(QObject *view) const;

signals:
    void modelChanged();

private:
    QPointer<ViewModelBase> m_model;
    QMetaObject::Connection m_destroyed;
};
