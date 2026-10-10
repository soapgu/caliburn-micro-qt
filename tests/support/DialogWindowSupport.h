#pragma once
#include <CaliburnMicroQt/WindowManager.h>
#include <QQmlEngine>
#include <QGuiApplication>
#include <QQuickWindow>
#include <QQuickItem>

inline QQuickWindow *dialogWindow(QQuickWindow *owner)
{
    for (auto *window : QGuiApplication::allWindows())
        if (window->objectName() == QStringLiteral("dialogWindow") && window->transientParent() == owner)
            return qobject_cast<QQuickWindow *>(window);
    return nullptr;
}
inline QQuickItem *dialogControl(QQuickWindow *owner, const QString &name)
{
    auto *window = dialogWindow(owner);
    return window ? window->findChild<QQuickItem *>(name) : nullptr;
}

// 通过公开展示入口建立关联，测试不接入外部窗口或访问私有状态。
inline QQuickWindow *managedWindow(QObject *model)
{
    for (auto *window : QGuiApplication::allWindows())
        if (window->property("viewModel").value<QObject *>() == model)
            return qobject_cast<QQuickWindow *>(window);
    return nullptr;
}

template<class Model>
QQuickWindow *showManagedWindow(WindowManager &manager, Model *model)
{
    return manager.showWindow(QVariant::fromValue(model)) ? managedWindow(model) : nullptr;
}

// 在借用的根 VM 离开作用域前释放窗口，断言提前返回时同样执行。
struct WindowCleanup
{
    WindowManager &manager;
    ~WindowCleanup() { manager.releaseWindows(); }
};
