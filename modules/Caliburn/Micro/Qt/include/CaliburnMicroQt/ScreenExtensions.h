#pragma once

#include <CaliburnMicroQt/IActivate.h>
#include <CaliburnMicroQt/IDeactivate.h>
#include <QObject>

namespace ScreenExtensions {

// 对齐 CM 3.2.0：按能力分派，空对象或未实现对应接口时无操作。
inline void tryActivate(QObject *item)
{
    if (auto *activator = qobject_cast<IActivate *>(item))
        activator->activate();
}

inline void tryDeactivate(QObject *item, bool close)
{
    if (auto *deactivator = qobject_cast<IDeactivate *>(item))
        deactivator->deactivate(close);
}

}
