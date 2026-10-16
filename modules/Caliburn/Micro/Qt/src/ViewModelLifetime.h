#pragma once

#include <QObject>
#include <QPointer>
#include <QSet>

// 仅协调 Conductor 已接管成员的 View 借用与回收；其他所有者不创建此对象。
class ViewModelLifetime final : public QObject
{
    Q_OBJECT
public:
    static ViewModelLifetime *find(QObject *model);
    static ViewModelLifetime *get(QObject *model);
    static bool isRetiring(QObject *model);
    void watchOwner(QObject *owner);
    void retainView(QObject *view);
    void retire();

signals:
    void releaseRequested();

private:
    explicit ViewModelLifetime(QObject *model);
    void scheduleDeletion();
    QPointer<QObject> m_model;
    QPointer<QObject> m_owner;
    QSet<QObject *> m_views;
    bool m_retiring = false;
    bool m_deletionScheduled = false;
};
