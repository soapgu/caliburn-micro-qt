#pragma once

#include <QObject>
#include <QPointer>

class ScreenViewModel;

// 只关联当前受管窗口，不拥有模型或窗口，不提供 View 缓存。
class WindowViewBinding final
{
public:
    WindowViewBinding(QObject *model, QObject *window, bool isDialog = false);
    ~WindowViewBinding();
    WindowViewBinding(const WindowViewBinding &) = delete;
    WindowViewBinding &operator=(const WindowViewBinding &) = delete;
    static bool canBind(QObject *model);
    void detach();

private:
    QPointer<ScreenViewModel> m_model;
};
