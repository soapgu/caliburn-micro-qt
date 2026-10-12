#include "FrameworkLog.h"
#include <CaliburnMicroQt/ViewRegistry.h>
#include <CaliburnMicroQt/ConfirmActionViewModel.h>
#include <QCoreApplication>
#include <QHash>
#include <QThread>

namespace {
struct RegistryData {
    QHash<const QMetaObject *, QUrl> views;
    const QHash<const QMetaObject *, QUrl> defaults {
        {&ConfirmActionViewModel::staticMetaObject,
         QUrl(QStringLiteral("qrc:/qt/qml/Caliburn/Micro/Qt/ConfirmActionView.qml"))}
    };
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
    qCWarning(FrameworkLog::view, "ViewRegistry: registration, freeze and lookup require the application thread");
    return false;
}
}

bool ViewRegistry::registerType(const QMetaObject &type, const QUrl &url)
{
    if (!isApplicationThread())
        return false;
    auto &data = registryData();
    if (data.frozen) {
        qCWarning(FrameworkLog::view, "ViewRegistry: registration rejected after freeze");
        return false;
    }
    if (url.isEmpty() || !url.isValid() || url.isRelative()) {
        qCWarning(FrameworkLog::view, "ViewRegistry: view URL must be valid, non-empty and absolute");
        return false;
    }
    const auto found = data.views.constFind(&type);
    if (found != data.views.cend()) {
        if (*found == url)
            return true;
        qCWarning(FrameworkLog::view).noquote() << "ViewRegistry: conflicting type mapping" << type.className();
        return false;
    }
    qCDebug(FrameworkLog::view).noquote() << "View registered; type:" << type.className() << "URL:" << url.toString();
    data.views.insert(&type, url);
    return true;
}

bool ViewRegistry::freeze()
{
    if (!isApplicationThread())
        return false;
    if (!registryData().frozen)
        qCDebug(FrameworkLog::view) << "View registry frozen; explicit mappings:" << registryData().views.size();
    registryData().frozen = true;
    return true;
}

QUrl ViewRegistry::viewUrl(const ViewModelBase *model)
{
    if (!isApplicationThread())
        return {};
    if (!model)
        return {};
    if (model->thread() != QThread::currentThread()) {
        qCWarning(FrameworkLog::view, "ViewRegistry: model must belong to the application thread");
        return {};
    }
    const auto &data = registryData();
    const auto found = data.views.constFind(model->metaObject());
    if (found != data.views.cend()) {
        qCDebug(FrameworkLog::view).noquote() << "View mapping found:" << FrameworkLog::identity(model) << "URL:" << found->toString();
        return *found;
    }
    const auto defaultView = data.defaults.constFind(model->metaObject());
    if (defaultView != data.defaults.cend()) {
        qCDebug(FrameworkLog::view).noquote() << "Default view mapping found:" << FrameworkLog::identity(model) << "URL:" << defaultView->toString();
        return *defaultView;
    }
    qCWarning(FrameworkLog::view).noquote() << "ViewRegistry: no view registered for type" << model->metaObject()->className();
    return {};
}
