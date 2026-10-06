#include <CaliburnMicroQt/ViewRegistry.h>
#include <QCoreApplication>
#include <QHash>
#include <QThread>

namespace {
struct RegistryData {
    QHash<const QMetaObject *, QUrl> views;
    bool frozen = false;
};

RegistryData &registryData()
{
    static RegistryData data;
    return data;
}

bool isApplicationThread()
{
    auto *app = QCoreApplication::instance();
    if (app && QThread::currentThread() == app->thread())
        return true;
    qWarning("ViewRegistry：登记与查询必须在应用主线程调用");
    return false;
}
}

bool ViewRegistry::registerType(const QMetaObject &type, const QUrl &url)
{
    if (!isApplicationThread())
        return false;
    auto &data = registryData();
    if (data.frozen) {
        qWarning("ViewRegistry：配置已冻结，拒绝登记");
        return false;
    }
    if (url.isEmpty() || !url.isValid() || url.isRelative()) {
        qWarning("ViewRegistry：View URL 必须为有效、非空的绝对地址");
        return false;
    }
    const auto found = data.views.constFind(&type);
    if (found != data.views.cend()) {
        if (*found == url)
            return true;
        qWarning().noquote() << "ViewRegistry：类型映射冲突" << type.className();
        return false;
    }
    data.views.insert(&type, url);
    return true;
}

QUrl ViewRegistry::viewUrl(const ViewModelBase *model)
{
    if (!isApplicationThread())
        return {};
    if (!model)
        return {};
    auto &data = registryData();
    data.frozen = true;
    if (model->thread() != QThread::currentThread()) {
        qWarning("ViewRegistry：模型必须位于应用主线程");
        return {};
    }
    const auto found = data.views.constFind(model->metaObject());
    if (found != data.views.cend())
        return *found;
    qWarning().noquote() << "ViewRegistry：未登记类型" << model->metaObject()->className();
    return {};
}
