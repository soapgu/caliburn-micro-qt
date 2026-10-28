#pragma once

#include <QColor>
#include <QString>

// 原生窗口标题与业务 View 中的正文标题分别配置。
struct DialogOptions
{
    QString title;
    // 无效颜色表示跟随系统 QPalette::Window；有效颜色固定用于弹窗客户区。
    QColor backgroundColor;
};
