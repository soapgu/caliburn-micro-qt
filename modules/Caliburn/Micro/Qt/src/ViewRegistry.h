#pragma once
#include <QHash>
#include <QMetaObject>
#include <QUrl>

// 内部精确映射表；ViewLocator 负责线程、配置与冻结检查。
class ViewRegistry
{
public:
    bool registerType(const QMetaObject &type, const QUrl &url);
    QUrl viewUrl(const QObject *model) const;
private:
    QHash<const QMetaObject *, QUrl> m_views;
};
