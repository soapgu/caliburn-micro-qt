#pragma once

#include <QObject>
#include <functional>

// 窗口服务自身实现的退出能力；所有方法在服务所属线程调用。
// 不接管借用 VM，不要求旧 IWindowManager 实现补充此接口。
class IWindowManagerLifecycle
{
public:
    virtual ~IWindowManagerLifecycle() = default;
    // 结束在途弹窗、解除交互关联，保留普通 View 供退出钩子访问。
    virtual void prepareForShutdown() = 0;
    // 释放窗口、View、引擎及模型借用，不删除借用 VM。
    // 允许重复清理，不重复执行已尝试的关闭生命周期。
    virtual void releaseWindows() = 0;
    // 同线程通知，绑定 context 寿命，返回可主动断开的连接。
    // 空 context、空回调或跨线程 context 返回无效连接。
    // 回调不得抛异常、销毁服务或重入清理。
    virtual QMetaObject::Connection subscribeWindowCleanupFailed(
            QObject *context, std::function<void()> callback) = 0;
};

Q_DECLARE_INTERFACE(IWindowManagerLifecycle, "Caliburn.Micro.Qt.IWindowManagerLifecycle/1.0")
