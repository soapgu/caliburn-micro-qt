#pragma once

#include <CaliburnMicroQt/ViewModelBase.h>
#include <QPointer>
#include <QUrl>

// 公开、可创建的 QML 借用保护辅助类型；页面装配优先使用 ViewHost。
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
    Q_INVOKABLE void reportLoadStarted(const QUrl &url);
    Q_INVOKABLE void reportLoadSucceeded(const QUrl &url) const;
    Q_INVOKABLE void reportLoadFailed(const QString &message) const;

signals:
    void modelChanged();

private:
    QString m_loadIdentity;
    QUrl m_loadUrl;
    QPointer<ViewModelBase> m_model;
    QMetaObject::Connection m_destroyed;
};
