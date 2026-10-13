#pragma once

#include <QObject>
#include <functional>

using DeactivationCallback = std::function<void(bool)>;

// close 表示关闭生命周期；对象回收仍由所有者负责。
class IDeactivate
{
public:
    virtual ~IDeactivate() = default;
    virtual void deactivate(bool close) = 0;
    // 同线程通知：执行前发尝试通知，正常完成后发完成通知。
    // 订阅绑定 context 寿命，返回可由调用方断开的连接。
    virtual QMetaObject::Connection subscribeAttemptingDeactivation(
            QObject *context, DeactivationCallback callback) = 0;
    virtual QMetaObject::Connection subscribeDeactivated(
            QObject *context, DeactivationCallback callback) = 0;
};

Q_DECLARE_INTERFACE(IDeactivate, "Caliburn.Micro.Qt.IDeactivate/2.0")
