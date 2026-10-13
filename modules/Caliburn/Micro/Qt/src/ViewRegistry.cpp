#include "FrameworkLog.h"
#include "ViewRegistry.h"

bool ViewRegistry::registerType(const QMetaObject &type, const QUrl &url)
{
    const auto found = m_views.constFind(&type);
    if (found != m_views.cend()) {
        if (*found == url)
            return true;
        qCWarning(FrameworkLog::view).noquote() << "ViewLocator: conflicting type mapping" << type.className();
        return false;
    }
    qCDebug(FrameworkLog::view).noquote() << "View registered; type:" << type.className() << "URL:" << url.toString();
    m_views.insert(&type, url);
    return true;
}

QUrl ViewRegistry::viewUrl(const QObject *model) const
{
    const auto found = m_views.constFind(model->metaObject());
    if (found != m_views.cend()) {
        qCDebug(FrameworkLog::view).noquote() << "View mapping found:" << FrameworkLog::identity(model) << "URL:" << found->toString();
        return *found;
    }
    return {};
}
