#include "ViewModelLifetime.h"
#include <QCoreApplication>

ViewModelLifetime::ViewModelLifetime(QObject *model) : QObject(model), m_model(model) {}

ViewModelLifetime *ViewModelLifetime::find(QObject *model)
{
    return model ? model->findChild<ViewModelLifetime *>(QString(), Qt::FindDirectChildrenOnly) : nullptr;
}

ViewModelLifetime *ViewModelLifetime::get(QObject *model)
{
    if (!model) return nullptr;
    auto *lifetime = find(model);
    return lifetime ? lifetime : new ViewModelLifetime(model);
}

bool ViewModelLifetime::isRetiring(QObject *model)
{
    auto *lifetime = find(model);
    return lifetime && lifetime->m_retiring;
}

void ViewModelLifetime::watchOwner(QObject *owner)
{
    m_owner = owner;
    connect(owner, &QObject::destroyed, this, [this] {
        // QObject 父树即将回收成员；仍有 View 借用时先转交给回收流程。
        if (!m_views.isEmpty()) retire();
    });
}

void ViewModelLifetime::retainView(QObject *view)
{
    if (!view || m_retiring || m_views.contains(view)) return;
    m_views.insert(view);
    connect(view, &QObject::destroyed, this, [this, view] {
        m_views.remove(view);
        scheduleDeletion();
    });
}

void ViewModelLifetime::retire()
{
    if (m_retiring || !m_model) return;
    m_retiring = true;
    // 管理者提前析构不能越过 View 的寿命。没有 View 时保留父树兜底。
    if (!m_views.isEmpty()) m_model->setParent(nullptr);
    emit releaseRequested();
    scheduleDeletion();
}

void ViewModelLifetime::scheduleDeletion()
{
    if (!m_retiring || !m_model || !m_views.isEmpty() || m_deletionScheduled) return;
    m_deletionScheduled = true;
    // 退出时事件循环可能已停止：View 释放后恢复父树兜底，避免独立对象滞留。
    if (!m_model->parent()) {
        auto *fallback = m_owner ? m_owner.data() : QCoreApplication::instance();
        if (fallback && fallback->thread() == m_model->thread()) m_model->setParent(fallback);
    }
    // destroyed 发出后 Qt 还会销毁 View 子树；延迟删除确保这些析构也能访问 VM。
    m_model->deleteLater();
}
