#include "FrameworkLog.h"
#include "ViewRegistry.h"
#include "ViewContext.h"

bool ViewRegistry::registerType(const QMetaObject &type, const QUrl &url, const std::optional<QString> &context)
{
    auto &mappings = m_views[&type];
    const auto found = mappings.constFind(context);
    if (found != mappings.cend()) {
        if (*found == url)
            return true;
        qCWarning(FrameworkLog::view).noquote() << "ViewLocator: conflicting type mapping" << type.className()
                                             << "context:" << ViewContext::describe(context);
        return false;
    }
    qCDebug(FrameworkLog::view).noquote() << "View registered; type:" << type.className()
                                       << "context:" << ViewContext::describe(context) << "URL:" << url.toString();
    mappings.insert(context, url);
    return true;
}

QUrl ViewRegistry::viewUrl(const QObject *model, const std::optional<QString> &context) const
{
    const auto found = m_views.constFind(model->metaObject());
    if (found != m_views.cend()) {
        const auto mapping = found->constFind(context);
        if (mapping == found->cend()) return {};
        qCDebug(FrameworkLog::view).noquote() << "View mapping found:" << FrameworkLog::identity(model)
                                           << "context:" << ViewContext::describe(context) << "URL:" << mapping->toString();
        return *mapping;
    }
    return {};
}
