#pragma once

#include <QObject>
#include <QVariant>
#include <functional>
#include "../../modules/Caliburn/Micro/Qt/src/WindowViewBinding.h"

// 无 GUI 的窗口关闭端点；与真实窗口使用同一个弱关联机制。
class TestCloseWindow : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariant dialogResult MEMBER result)
public:
    QVariant result;
    int calls = 0;
    std::function<void()> action;
    Q_INVOKABLE void close()
    {
        ++calls;
        auto callback = action;
        if (callback) callback();
    }
};
