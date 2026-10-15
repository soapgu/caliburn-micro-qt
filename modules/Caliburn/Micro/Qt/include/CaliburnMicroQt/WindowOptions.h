#pragma once

#include <QPointer>
#include <QScreen>
#include <QRect>
#include <QWindow>
#include <optional>

enum class WindowRole { Primary, Auxiliary };

// 仅描述展示配置；屏幕为弱引用，窗口和 VM 的所有权契约不变。
struct WindowOptions
{
    WindowRole role = WindowRole::Primary;
    QPointer<QScreen> screen;
    std::optional<QRect> geometry;
    std::optional<Qt::WindowFlags> flags;
    std::optional<QWindow::Visibility> visibility;
    bool activateOnShow = false;
};
