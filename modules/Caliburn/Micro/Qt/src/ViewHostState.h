#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>
#include <QPointer>
#include <QUrl>

// 公开、可创建的 QML 借用保护辅助类型；页面装配优先使用 ViewHost。
class ViewHostState : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QObject *model READ model WRITE setModel NOTIFY modelChanged)

public:
    explicit ViewHostState(QObject *parent = nullptr) : QObject(parent) {}
    QObject *model() const { return m_model.data(); }
    void setModel(QObject *model);
    Q_INVOKABLE bool matchesView(QObject *view) const;
    Q_INVOKABLE void retainView(QObject *view);
    Q_INVOKABLE void reportLoadStarted(const QUrl &url);
    Q_INVOKABLE void reportLoadSucceeded(const QUrl &url) const;
    Q_INVOKABLE void reportLoadFailed(const QString &message) const;

signals:
    void modelChanged();

private:
    QString m_loadIdentity;
    QUrl m_loadUrl;
    QPointer<QObject> m_model;
    QMetaObject::Connection m_destroyed;
    QMetaObject::Connection m_releaseRequested;
};
